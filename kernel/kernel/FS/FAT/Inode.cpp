#include <BAN/Time.h>

#include <kernel/FS/FAT/FileSystem.h>
#include <kernel/FS/FAT/Inode.h>

#include <ctype.h>

namespace Kernel
{

	static uint64_t fat_date_to_epoch(FAT::Date date, FAT::Time time)
	{
		BAN::Time ban_time {};

		ban_time.year  =  (date.year % 128) + 1980;
		ban_time.month = ((date.month - 1) % 12) + 1;
		ban_time.day   = ((date.day - 1) % 31) + 1;

		ban_time.hour   = (time.hour   % 24);
		ban_time.minute = (time.minute % 60);
		ban_time.second = (time.second % 30) * 2;

		return BAN::to_unix_time(ban_time);
	}

	blksize_t FATInode::blksize() const
	{
		return m_fs.inode_block_size(this);
	}

	timespec FATInode::atime() const
	{
		uint64_t epoch = fat_date_to_epoch(m_entry.last_access_date, {});
		return timespec { .tv_sec = epoch, .tv_nsec = 0 };
	}

	timespec FATInode::mtime() const
	{
		uint64_t epoch = fat_date_to_epoch(m_entry.write_date, m_entry.write_time);
		return timespec { .tv_sec = epoch, .tv_nsec = 0 };
	}

	timespec FATInode::ctime() const
	{
		uint64_t epoch = fat_date_to_epoch(m_entry.creation_date, m_entry.creation_time);
		return timespec { .tv_sec = epoch, .tv_nsec = 0 };
	}

	const FileSystem* FATInode::filesystem() const
	{
		return &m_fs;
	}

	BAN::ErrorOr<void> FATInode::for_each_directory_entry(BAN::ConstByteSpan entry_span, BAN::Function<BAN::Iteration(const FAT::DirectoryEntry&)> callback)
	{
		ASSERT(mode().ifdir());

		auto directory_entries = entry_span.as_span<const FAT::DirectoryEntry>();
		for (uint32_t i = 0; i < directory_entries.size(); i++)
		{
			const auto& directory_entry = directory_entries[i];
			if ((uint8_t)directory_entry.name[0] == 0xE5)
				continue;
			if (directory_entry.name[0] == 0)
				break;
			if ((directory_entry.attr & 0x3F) == 0x0F)
				continue;
			if (callback(directory_entry) == BAN::Iteration::Break)
				return {};
		}

		return {};
	}

	BAN::ErrorOr<void> FATInode::for_each_directory_entry(BAN::ConstByteSpan entry_span, BAN::Function<BAN::Iteration(const FAT::DirectoryEntry&, BAN::String, uint32_t)> callback)
	{
		ASSERT(mode().ifdir());

		BAN::String long_name;

		auto directory_entries = entry_span.as_span<const FAT::DirectoryEntry>();
		auto long_name_entries = entry_span.as_span<const FAT::LongNameEntry>();
		for (uint32_t i = 0; i < directory_entries.size(); i++)
		{
			const auto& directory_entry = directory_entries[i];
			if ((uint8_t)directory_entry.name[0] == 0xE5)
				continue;
			if (directory_entry.name[0] == 0)
				break;

			if ((directory_entry.attr & 0x3F) == 0x0F)
			{
				if (!long_name.empty())
				{
					dwarnln("Invalid long name entry");
					continue;
				}

				const auto& long_name_entry = long_name_entries[i];
				if (!(long_name_entry.order & 0x40))
				{
					dwarnln("Invalid long name entry");
					continue;
				}

				const uint32_t long_name_entry_count = long_name_entry.order & ~0x40;
				if (i + long_name_entry_count >= directory_entries.size())
				{
					dwarnln("Invalid long name entry");
					continue;
				}

				for (uint32_t j = 0; j < long_name_entry_count; j++)
					TRY(long_name.insert(long_name_entries[i + j].name_as_string(), 0));
				i += long_name_entry_count - 1;
				continue;
			}

			auto ret = callback(directory_entry, BAN::move(long_name), i);
			if (ret == BAN::Iteration::Break)
				return {};
			long_name.clear();
		}

		return {};
	}

	BAN::ErrorOr<BAN::RefPtr<Inode>> FATInode::find_inode_impl(BAN::StringView name)
	{
		ASSERT(mode().ifdir());

		BAN::Vector<uint8_t> cluster_buffer;
		TRY(cluster_buffer.resize(blksize()));
		auto cluster_span = BAN::ByteSpan(cluster_buffer.span());

		for (uint32_t cluster_index = 0;; cluster_index++)
		{
			// Returns ENOENT if this cluster is out of bounds
			TRY(m_fs.inode_read_cluster(this, cluster_index, cluster_span));

			auto error = BAN::Error::from_errno(0);
			BAN::RefPtr<FATInode> result;

			TRY(for_each_directory_entry(cluster_span,
				[&](const FAT::DirectoryEntry& entry, BAN::String long_name, uint32_t entry_index)
				{
					BAN::String file_name = long_name.empty() ? entry.name_as_string() : BAN::move(long_name);

					if (file_name.size() != name.size())
						return BAN::Iteration::Continue;

					for (size_t i = 0; i < name.size(); i++)
						if (tolower(name[i]) != tolower(file_name[i]))
							return BAN::Iteration::Continue;

					auto new_inode = m_fs.open_inode(this, entry, cluster_index, entry_index);
					if (new_inode.is_error())
						error = new_inode.release_error();
					else
						result = new_inode.release_value();
					return BAN::Iteration::Break;
				}
			));

			if (error.get_error_code())
				return error;
			if (result)
				return BAN::RefPtr<Inode>(result);
		}

		return BAN::Error::from_errno(ENOENT);
	}

	BAN::ErrorOr<size_t> FATInode::list_next_inodes_impl(off_t offset, struct dirent* list, size_t list_size)
	{
		ASSERT(mode().ifdir());
		ASSERT(offset >= 0);

		BAN::Vector<uint8_t> cluster_buffer;
		TRY(cluster_buffer.resize(blksize()));
		auto cluster_span = BAN::ByteSpan(cluster_buffer.span());

		{
			auto maybe_error = m_fs.inode_read_cluster(this, offset, cluster_span);
			if (maybe_error.is_error())
			{
				if (maybe_error.error().get_error_code() == ENOENT)
					return 0;
				return maybe_error.release_error();
			}
		}

		size_t valid_entry_count = 0;
		TRY(for_each_directory_entry(cluster_span,
			[&](const FAT::DirectoryEntry&)
			{
				valid_entry_count++;
				return BAN::Iteration::Continue;
			}
		));

		if (valid_entry_count > list_size)
			return BAN::Error::from_errno(ENOBUFS);

		TRY(for_each_directory_entry(cluster_span,
			[&](const FAT::DirectoryEntry& entry, const BAN::String& long_name, uint32_t)
			{
				BAN::String name = long_name.empty() ? entry.name_as_string() : BAN::move(long_name);
				list->d_ino = 0;
				list->d_type = (entry.attr & FAT::FileAttr::DIRECTORY) ? DT_DIR : DT_REG;
				strncpy(list->d_name, name.data(), sizeof(list->d_name));
				list++;
				return BAN::Iteration::Continue;
			}
		));

		return valid_entry_count;
	}

	BAN::ErrorOr<size_t> FATInode::read_impl(off_t s_offset, BAN::ByteSpan buffer)
	{
		ASSERT(s_offset >= 0);
		uint32_t offset = s_offset;

		if (offset >= m_entry.file_size)
			return 0;
		if (offset + buffer.size() > m_entry.file_size)
			buffer = buffer.slice(0, m_entry.file_size - offset);

		BAN::Vector<uint8_t> cluster_buffer;
		TRY(cluster_buffer.resize(blksize()));
		auto cluster_span = BAN::ByteSpan(cluster_buffer.span());

		const uint32_t block_size = blksize();

		size_t nread = 0;
		if (auto rem = offset % block_size)
		{
			const uint32_t to_read = BAN::Math::min<uint32_t>(buffer.size(), block_size - rem);

			if (auto ret = m_fs.inode_read_cluster(this, offset / block_size, cluster_span); ret.is_error())
			{
				if (ret.error().get_error_code() == ENOENT)
					return nread;
				return ret.release_error();
			}
			memcpy(buffer.data(), cluster_span.data() + rem, to_read);

			nread += to_read;
			offset += to_read;
			buffer = buffer.slice(to_read);
		}

		while (buffer.size() >= block_size)
		{
			if (auto ret = m_fs.inode_read_cluster(this, offset / block_size, buffer); ret.is_error())
			{
				if (ret.error().get_error_code() == ENOENT)
					return nread;
				return ret.release_error();
			}

			nread += block_size;
			offset += block_size;
			buffer = buffer.slice(block_size);
		}

		if (buffer.size() > 0)
		{
			if (auto ret = m_fs.inode_read_cluster(this, offset / block_size, cluster_span); ret.is_error())
			{
				if (ret.error().get_error_code() == ENOENT)
					return nread;
				return ret.release_error();
			}
			memcpy(buffer.data(), cluster_span.data(), buffer.size());

			nread += buffer.size();
			offset += buffer.size();
			buffer = buffer.slice(buffer.size());
		}

		return nread;
	}

}
