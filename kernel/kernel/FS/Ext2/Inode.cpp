#include <BAN/Function.h>
#include <BAN/ScopeGuard.h>
#include <kernel/FS/Ext2/FileSystem.h>
#include <kernel/FS/Ext2/Inode.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	blksize_t Ext2Inode::blksize() const
	{
		return m_fs.block_size();
	}

	blkcnt_t Ext2Inode::blocks() const
	{
		return m_inode.blocks / (2 << m_fs.superblock().log_block_size);
	}

	uint32_t Ext2Inode::block_group() const
	{
		return (m_ino - 1) / m_fs.superblock().blocks_per_group;
	}

	BAN::ErrorOr<BAN::RefPtr<Ext2Inode>> Ext2Inode::create(Ext2FS& fs, uint32_t inode_ino)
	{
		if (fs.inode_cache().contains(inode_ino))
			return fs.inode_cache()[inode_ino];

		auto inode_location = TRY(fs.locate_inode(inode_ino));

		auto block_buffer = fs.get_block_buffer();
		TRY(fs.read_block(inode_location.block, block_buffer));

		auto& inode = block_buffer.span().slice(inode_location.offset).as<Ext2::Inode>();

		auto result = TRY(BAN::RefPtr<Ext2Inode>::create(fs, inode, inode_ino));
		TRY(fs.inode_cache().insert(inode_ino, result));
		return result;
	}

	Ext2Inode::~Ext2Inode()
	{
		if (m_inode.links_count > 0)
			return;
		if (auto ret = cleanup_from_fs(); ret.is_error())
			dwarnln("Could not cleanup inode from FS: {}", ret.error());
	}

	BAN::ErrorOr<BAN::Optional<uint32_t>> Ext2Inode::block_from_indirect_block(uint32_t block, uint32_t index, uint32_t depth)
	{
		if (block == 0)
			return BAN::Optional<uint32_t>();
		ASSERT(depth >= 1);

		auto block_buffer = m_fs.get_block_buffer();
		TRY(m_fs.read_block(block, block_buffer));

		const uint32_t indices_per_block = blksize() / sizeof(uint32_t);

		const uint32_t divisor = (depth > 1) ? indices_per_block * (depth - 1) : 1;

		const uint32_t next_block = block_buffer.span().as_span<uint32_t>()[(index / divisor) % indices_per_block];
		if (next_block == 0)
			return BAN::Optional<uint32_t>();
		if (depth == 1)
			return BAN::Optional<uint32_t>(next_block);

		return block_from_indirect_block(next_block, index, depth - 1);
	}

	BAN::ErrorOr<BAN::Optional<uint32_t>> Ext2Inode::fs_block_of_data_block_index(uint32_t data_block_index)
	{
		const uint32_t indices_per_block = blksize() / sizeof(uint32_t);

		if (data_block_index < 12)
		{
			if (m_inode.block[data_block_index] == 0)
				return BAN::Optional<uint32_t>();
			return BAN::Optional<uint32_t>(m_inode.block[data_block_index]);
		}
		data_block_index -= 12;

		if (data_block_index < indices_per_block)
			return block_from_indirect_block(m_inode.block[12], data_block_index, 1);
		data_block_index -= indices_per_block;

		if (data_block_index < indices_per_block * indices_per_block)
			return block_from_indirect_block(m_inode.block[13], data_block_index, 2);
		data_block_index -= indices_per_block * indices_per_block;

		if (data_block_index < indices_per_block * indices_per_block * indices_per_block)
			return block_from_indirect_block(m_inode.block[14], data_block_index, 3);

		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<BAN::String> Ext2Inode::link_target_impl()
	{
		ASSERT(mode().iflnk());
		if (m_inode.size < sizeof(m_inode.block))
			return BAN::String((const char*)m_inode.block);
		return BAN::Error::from_errno(ENOTSUP);
	}

	BAN::ErrorOr<size_t> Ext2Inode::read_impl(off_t offset, BAN::ByteSpan buffer)
	{
		// FIXME: update atime if needed

		ASSERT(!mode().ifdir());
		ASSERT(offset >= 0);

		if (static_cast<BAN::make_unsigned_t<decltype(offset)>>(offset) >= UINT32_MAX || buffer.size() >= UINT32_MAX || buffer.size() >= (size_t)(UINT32_MAX - offset))
			return BAN::Error::from_errno(EOVERFLOW);

		if (static_cast<BAN::make_unsigned_t<decltype(offset)>>(offset) >= m_inode.size)
			return 0;

		uint32_t count = buffer.size();
		if (offset + buffer.size() > m_inode.size)
			count = m_inode.size - offset;

		const uint32_t block_size = blksize();

		auto block_buffer = m_fs.get_block_buffer();

		const uint32_t first_block = offset / block_size;
		const uint32_t last_block = BAN::Math::div_round_up<uint32_t>(offset + count, block_size);

		size_t n_read = 0;

		for (uint32_t data_block_index = first_block; data_block_index < last_block; data_block_index++)
		{
			auto block_index = TRY(fs_block_of_data_block_index(data_block_index));
			if (block_index.has_value())
				TRY(m_fs.read_block(block_index.value(), block_buffer));
			else
				memset(block_buffer.data(), 0x00, block_buffer.size());

			uint32_t copy_offset = (offset + n_read) % block_size;
			uint32_t to_copy = BAN::Math::min<uint32_t>(block_size - copy_offset, count - n_read);
			memcpy(buffer.data() + n_read, block_buffer.data() + copy_offset, to_copy);

			n_read += to_copy;
		}

		return n_read;
	}

	BAN::ErrorOr<size_t> Ext2Inode::write_impl(off_t offset, BAN::ConstByteSpan buffer)
	{
		// FIXME: update atime if needed

		ASSERT(!mode().ifdir());
		ASSERT(offset >= 0);

		if (static_cast<BAN::make_unsigned_t<decltype(offset)>>(offset) >= UINT32_MAX || buffer.size() >= UINT32_MAX || buffer.size() >= (size_t)(UINT32_MAX - offset))
			return BAN::Error::from_errno(EOVERFLOW);

		if (m_inode.size < offset + buffer.size())
			TRY(truncate_impl(offset + buffer.size()));

		const uint32_t block_size = blksize();

		auto block_buffer = m_fs.get_block_buffer();

		size_t written = 0;
		size_t to_write = buffer.size();

		// Write partial block
		if (offset % block_size)
		{
			auto block_index = TRY(fs_block_of_data_block_index(offset / block_size));
			if (block_index.has_value())
				TRY(m_fs.read_block(block_index.value(), block_buffer));
			else
			{
				block_index = TRY(allocate_new_block(offset / block_size));;
				memset(block_buffer.data(), 0x00, block_buffer.size());
			}

			uint32_t block_offset = offset % block_size;
			uint32_t to_copy = BAN::Math::min<uint32_t>(block_size - block_offset, to_write);

			memcpy(block_buffer.data() + block_offset, buffer.data(), to_copy);
			TRY(m_fs.write_block(block_index.value(), block_buffer));

			written += to_copy;
			offset += to_copy;
			to_write -= to_copy;
		}

		while (to_write >= block_size)
		{
			auto block_index = TRY(fs_block_of_data_block_index(offset / block_size));
			if (!block_index.has_value())
				block_index = TRY(allocate_new_block(offset / block_size));

			memcpy(block_buffer.data(), buffer.data() + written, block_buffer.size());
			TRY(m_fs.write_block(block_index.value(), block_buffer));

			written += block_size;
			offset += block_size;
			to_write -= block_size;
		}

		if (to_write > 0)
		{
			auto block_index = TRY(fs_block_of_data_block_index(offset / block_size));
			if (block_index.has_value())
				TRY(m_fs.read_block(block_index.value(), block_buffer));
			else
			{
				block_index = TRY(allocate_new_block(offset / block_size));
				memset(block_buffer.data(), 0x00, block_buffer.size());
			}

			memcpy(block_buffer.data(), buffer.data() + written, to_write);
			TRY(m_fs.write_block(block_index.value(), block_buffer));
		}

		return buffer.size();
	}

	BAN::ErrorOr<void> Ext2Inode::truncate_impl(size_t new_size)
	{
		if (m_inode.size == new_size)
			return {};

		// TODO: we should remove unused blocks on shrink

		const auto old_size = m_inode.size;

		m_inode.size = new_size;
		if (auto ret = sync(); ret.is_error())
		{
			m_inode.size = old_size;
			return ret.release_error();
		}

		return {};
	}

	BAN::ErrorOr<void> Ext2Inode::chmod_impl(mode_t mode)
	{
		ASSERT((mode & Inode::Mode::TYPE_MASK) == 0);
		if (m_inode.mode == mode)
			return {};

		const auto old_mode = m_inode.mode;

		m_inode.mode = (m_inode.mode & Inode::Mode::TYPE_MASK) | mode;
		if (auto ret = sync(); ret.is_error())
		{
			m_inode.mode = old_mode;
			return ret.release_error();
		}

		return {};
	}

	BAN::ErrorOr<void> Ext2Inode::cleanup_indirect_block(uint32_t block, uint32_t depth)
	{
		ASSERT(block);

		if (depth == 0)
		{
			TRY(m_fs.release_block(block));
			return {};
		}

		auto block_buffer = m_fs.get_block_buffer();
		TRY(m_fs.read_block(block, block_buffer));

		const uint32_t ids_per_block = blksize() / sizeof(uint32_t);
		for (uint32_t i = 0; i < ids_per_block; i++)
		{
			const uint32_t next_block = block_buffer.span().as_span<uint32_t>()[i];
			if (next_block == 0)
				continue;
			TRY(cleanup_indirect_block(next_block, depth - 1));
		}

		TRY(m_fs.release_block(block));
		return {};
	}

	BAN::ErrorOr<void> Ext2Inode::cleanup_from_fs()
	{
		ASSERT(m_inode.links_count == 0);

		if (mode().iflnk() && (size_t)size() < sizeof(m_inode.block))
			goto done;

		// cleanup direct blocks
		for (uint32_t i = 0; i < 12; i++)
			if (m_inode.block[i])
				TRY(m_fs.release_block(m_inode.block[i]));

		// cleanup indirect blocks
		if (m_inode.block[12])
			TRY(cleanup_indirect_block(m_inode.block[12], 1));
		if (m_inode.block[13])
			TRY(cleanup_indirect_block(m_inode.block[13], 2));
		if (m_inode.block[14])
			TRY(cleanup_indirect_block(m_inode.block[14], 3));

done:
		// mark blocks as deleted
		memset(m_inode.block, 0x00, sizeof(m_inode.block));

		// FIXME: this is only required since fs does not get
		//        deleting inode from its cache
		TRY(sync());

		TRY(m_fs.delete_inode(ino()));

		return {};
	}

	BAN::ErrorOr<size_t> Ext2Inode::list_next_inodes_impl(off_t offset, struct dirent* list, size_t list_size)
	{
		ASSERT(mode().ifdir());
		ASSERT(offset >= 0);

		if (static_cast<BAN::make_unsigned_t<decltype(offset)>>(offset) >= max_used_data_block_count())
			return 0;

		// FIXME: can we actually assume directories have all their blocks allocated
		const uint32_t block_index = TRY(fs_block_of_data_block_index(offset)).value();

		auto block_buffer = m_fs.get_block_buffer();

		TRY(m_fs.read_block(block_index, block_buffer));

		// First determine if we have big enough list
		size_t entry_count = 0;
		{
			BAN::ConstByteSpan entry_span = block_buffer.span();

			while (entry_span.size() >= sizeof(Ext2::LinkedDirectoryEntry))
			{
				auto& entry = entry_span.as<const Ext2::LinkedDirectoryEntry>();
				if (entry.inode)
					entry_count++;
				entry_span = entry_span.slice(entry.rec_len);
			}

			if (entry_count > list_size)
				return BAN::Error::from_errno(ENOBUFS);
		}

		if (entry_count == 0)
			return BAN::Error::from_errno(ENODATA);

		// Second fill the list
		{
			dirent* dirp = list;

			BAN::ConstByteSpan entry_span = block_buffer.span();
			while (entry_span.size() >= sizeof(Ext2::LinkedDirectoryEntry))
			{
				auto& entry = entry_span.as<const Ext2::LinkedDirectoryEntry>();
				if (entry.inode)
				{
					dirp->d_ino = entry.inode;
					dirp->d_type = entry.file_type;
					const size_t name_len = BAN::Math::min<size_t>(entry.name_len, sizeof(dirp->d_name) - 1);
					strncpy(dirp->d_name, entry.name, name_len);
					dirp->d_name[name_len] = '\0';
					dirp++;
				}
				entry_span = entry_span.slice(entry.rec_len);
			}
		}

		return entry_count;
	}

	static bool mode_has_valid_type(mode_t mode)
	{
		switch (mode & Inode::Mode::TYPE_MASK)
		{
			case Inode::Mode::IFIFO: return true;
			case Inode::Mode::IFCHR: return true;
			case Inode::Mode::IFDIR: return true;
			case Inode::Mode::IFBLK: return true;
			case Inode::Mode::IFREG: return true;
			case Inode::Mode::IFLNK: return true;
			case Inode::Mode::IFSOCK: return true;
		}
		return false;
	}

	static Ext2::Inode initialize_new_inode_info(mode_t mode, uid_t uid, gid_t gid)
	{
		ASSERT(mode_has_valid_type(mode));

		timespec current_time = SystemTimer::get().real_time();
		return Ext2::Inode
		{
			.mode			= (uint16_t)mode,
			.uid			= (uint16_t)uid,
			.size			= 0,
			.atime 			= (uint32_t)current_time.tv_sec,
			.ctime 			= (uint32_t)current_time.tv_sec,
			.mtime 			= (uint32_t)current_time.tv_sec,
			.dtime 			= 0,
			.gid			= (uint16_t)gid,
			.links_count	= 0,
			.blocks			= 0,
			.flags			= 0,
			.osd1			= 0,
			.block 			= {},
			.generation		= 0,
			.file_acl		= 0,
			.dir_acl		= 0,
			.faddr			= 0,
			.osd2 			= {}
		};
	}

	BAN::ErrorOr<void> Ext2Inode::create_file_impl(BAN::StringView name, mode_t mode, uid_t uid, gid_t gid)
	{
		ASSERT(this->mode().ifdir());

		if (!(Mode(mode).ifreg()))
			return BAN::Error::from_errno(ENOTSUP);

		const uint32_t new_ino = TRY(m_fs.create_inode(initialize_new_inode_info(mode, uid, gid)));

		auto inode_or_error = Ext2Inode::create(m_fs, new_ino);
		if (inode_or_error.is_error())
		{
			TRY(m_fs.delete_inode(new_ino));
			return inode_or_error.release_error();
		}

		auto inode = inode_or_error.release_value();

		TRY(link_inode_to_directory(*inode, name));

		return {};
	}

	BAN::ErrorOr<void> Ext2Inode::create_directory_impl(BAN::StringView name, mode_t mode, uid_t uid, gid_t gid)
	{
		ASSERT(this->mode().ifdir());
		ASSERT(Mode(mode).ifdir());

		const uint32_t new_ino = TRY(m_fs.create_inode(initialize_new_inode_info(mode, uid, gid)));

		auto inode_or_error = Ext2Inode::create(m_fs, new_ino);
		if (inode_or_error.is_error())
		{
			TRY(m_fs.delete_inode(new_ino));
			return inode_or_error.release_error();
		}

		auto inode = inode_or_error.release_value();

		// link . and ..
		if (auto ret = inode->link_inode_to_directory(*inode, "."_sv); ret.is_error())
			return ({ TRY(inode->cleanup_from_fs()); ret.release_error(); });
		if (auto ret = inode->link_inode_to_directory(*this, ".."_sv); ret.is_error())
			return ({ TRY(inode->cleanup_from_fs()); ret.release_error(); });

		// link to parent
		if (auto ret = link_inode_to_directory(*inode, name); ret.is_error())
			return ({ TRY(inode->cleanup_from_fs()); ret.release_error(); });

		return {};
	}

	BAN::ErrorOr<void> Ext2Inode::link_inode_to_directory(Ext2Inode& inode, BAN::StringView name)
	{
		if (!this->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);

		if (name.size() > 255)
			return BAN::Error::from_errno(ENAMETOOLONG);

		if (m_inode.flags & Ext2::Enum::INDEX_FL)
		{
			dwarnln("file creation to indexed directory not supported");
			return BAN::Error::from_errno(ENOTSUP);
		}

		auto error_or = find_inode_impl(name);
		if (!error_or.is_error())
			return BAN::Error::from_errno(EEXISTS);
		if (error_or.error().get_error_code() != ENOENT)
			return error_or.error();

		const uint32_t block_size = m_fs.block_size();

		auto block_buffer = m_fs.get_block_buffer();

		auto write_inode =
			[&](uint32_t entry_offset, uint32_t entry_rec_len) -> BAN::ErrorOr<void>
			{
				auto typed_mode = inode.mode();
				uint8_t file_type = (m_fs.superblock().rev_level == Ext2::Enum::GOOD_OLD_REV) ? 0
					: typed_mode.ifreg()  ? Ext2::Enum::REG_FILE
					: typed_mode.ifdir()  ? Ext2::Enum::DIR
					: typed_mode.ifchr()  ? Ext2::Enum::CHRDEV
					: typed_mode.ifblk()  ? Ext2::Enum::BLKDEV
					: typed_mode.ififo()  ? Ext2::Enum::FIFO
					: typed_mode.ifsock() ? Ext2::Enum::SOCK
					: typed_mode.iflnk()  ? Ext2::Enum::SYMLINK
					: 0;

				auto& new_entry = block_buffer.span().slice(entry_offset).as<Ext2::LinkedDirectoryEntry>();
				new_entry.inode = inode.ino();
				new_entry.rec_len = entry_rec_len;
				new_entry.name_len = name.size();
				new_entry.file_type = file_type;
				memcpy(new_entry.name, name.data(), name.size());

				inode.m_inode.links_count++;
				TRY(inode.sync());

				return {};
			};

		uint32_t block_index = 0;
		uint32_t entry_offset = 0;

		uint32_t needed_entry_len = sizeof(Ext2::LinkedDirectoryEntry) + name.size();
		if (auto rem = needed_entry_len % 4)
			needed_entry_len += 4 - rem;

		// FIXME: can we actually assume directories have all their blocks allocated
		const uint32_t data_block_count = max_used_data_block_count();
		if (data_block_count == 0)
			goto needs_new_block;

		// Try to insert inode to last data block
		block_index = TRY(fs_block_of_data_block_index(data_block_count - 1)).value();
		TRY(m_fs.read_block(block_index, block_buffer));

		while (entry_offset < block_size)
		{
			auto& entry = block_buffer.span().slice(entry_offset).as<Ext2::LinkedDirectoryEntry>();

			uint32_t entry_min_rec_len = sizeof(Ext2::LinkedDirectoryEntry) + entry.name_len;
			if (auto rem = entry_min_rec_len % 4)
				entry_min_rec_len += 4 - rem;

			if (entry.inode == 0 && needed_entry_len <= entry.rec_len)
			{
				TRY(write_inode(entry_offset, entry.rec_len));
				TRY(m_fs.write_block(block_index, block_buffer));
				return {};
			}
			else if (needed_entry_len <= entry.rec_len - entry_min_rec_len)
			{
				uint32_t new_rec_len = entry.rec_len - entry_min_rec_len;
				entry.rec_len = entry_min_rec_len;

				TRY(write_inode(entry_offset + entry.rec_len, new_rec_len));
				TRY(m_fs.write_block(block_index, block_buffer));
				return {};
			}

			entry_offset += entry.rec_len;
		}

needs_new_block:
		block_index = TRY(allocate_new_block(data_block_count));
		m_inode.size += blksize();

		memset(block_buffer.data(), 0x00, block_buffer.size());
		TRY(write_inode(0, block_size));
		TRY(m_fs.write_block(block_index, block_buffer));

		return {};
	}

	BAN::ErrorOr<bool> Ext2Inode::is_directory_empty()
	{
		ASSERT(mode().ifdir());

		auto block_buffer = m_fs.get_block_buffer();

		// Confirm that this doesn't contain anything else than '.' or '..'
		for (uint32_t i = 0; i < max_used_data_block_count(); i++)
		{
			// FIXME: can we actually assume directories have all their blocks allocated
			const uint32_t block_index = TRY(fs_block_of_data_block_index(i)).value();
			TRY(m_fs.read_block(block_index, block_buffer));

			blksize_t offset = 0;
			while (offset < blksize())
			{
				auto& entry = block_buffer.span().slice(offset).as<Ext2::LinkedDirectoryEntry>();

				if (entry.inode)
				{
					BAN::StringView entry_name(entry.name, entry.name_len);
					if (entry_name != "."_sv && entry_name != ".."_sv)
						return false;
				}

				offset += entry.rec_len;
			}
		}

		return true;
	}

	BAN::ErrorOr<void> Ext2Inode::cleanup_default_links()
	{
		ASSERT(mode().ifdir());
		if (m_inode.flags & Ext2::Enum::INDEX_FL)
		{
			dwarnln("deletion of indexed directory is not supported");
			return BAN::Error::from_errno(ENOTSUP);
		}

		auto block_buffer = m_fs.get_block_buffer();

		for (uint32_t i = 0; i < max_used_data_block_count(); i++)
		{
			// FIXME: can we actually assume directories have all their blocks allocated
			const uint32_t block_index = TRY(fs_block_of_data_block_index(i)).value();
			TRY(m_fs.read_block(block_index, block_buffer));

			bool modified = false;

			blksize_t offset = 0;
			while (offset < blksize())
			{
				auto& entry = block_buffer.span().slice(offset).as<Ext2::LinkedDirectoryEntry>();

				if (entry.inode)
				{
					BAN::StringView entry_name(entry.name, entry.name_len);

					if (entry_name == "."_sv)
					{
						m_inode.links_count--;
						TRY(sync());
					}
					else if (entry_name == ".."_sv)
					{
						auto parent = TRY(Ext2Inode::create(m_fs, entry.inode));
						parent->m_inode.links_count--;
						TRY(parent->sync());
					}
					else
						ASSERT_NOT_REACHED();

					modified = true;
					entry.inode = 0;
				}

				offset += entry.rec_len;
			}

			if (modified)
				TRY(m_fs.write_block(block_index, block_buffer));
		}

		return {};
	}

	BAN::ErrorOr<void> Ext2Inode::unlink_impl(BAN::StringView name)
	{
		ASSERT(mode().ifdir());
		if (m_inode.flags & Ext2::Enum::INDEX_FL)
		{
			dwarnln("deletion from indexed directory is not supported");
			return BAN::Error::from_errno(ENOTSUP);
		}

		auto block_buffer = m_fs.get_block_buffer();

		for (uint32_t i = 0; i < max_used_data_block_count(); i++)
		{
			// FIXME: can we actually assume directories have all their blocks allocated
			const uint32_t block_index = TRY(fs_block_of_data_block_index(i)).value();
			TRY(m_fs.read_block(block_index, block_buffer));

			blksize_t offset = 0;
			while (offset < blksize())
			{
				auto& entry = block_buffer.span().slice(offset).as<Ext2::LinkedDirectoryEntry>();
				if (entry.inode && name == BAN::StringView(entry.name, entry.name_len))
				{
					auto inode = TRY(Ext2Inode::create(m_fs, entry.inode));
					if (inode->mode().ifdir())
					{
						if (!TRY(inode->is_directory_empty()))
							return BAN::Error::from_errno(ENOTEMPTY);
						TRY(inode->cleanup_default_links());
					}

					if (inode->nlink() == 0)
						dprintln("Corrupted filesystem. Deleting inode with 0 links");
					else
						inode->m_inode.links_count--;

					TRY(sync());

					// NOTE: If this was the last link to inode we must
					//       remove it from inode cache to trigger cleanup
					if (inode->nlink() == 0)
					{
						auto& cache = m_fs.inode_cache();
						if (cache.contains(inode->ino()))
							cache.remove(inode->ino());
					}

					// FIXME: This should expand the last inode if exists
					entry.inode = 0;
					TRY(m_fs.write_block(block_index, block_buffer));
				}
				offset += entry.rec_len;
			}
		}

		return {};
	}

	BAN::ErrorOr<uint32_t> Ext2Inode::allocate_new_block_to_indirect_block(uint32_t& block, uint32_t index, uint32_t depth)
	{
		const uint32_t inode_blocks_per_fs_block = blksize() / 512;
		const uint32_t indices_per_fs_block = blksize() / sizeof(uint32_t);

		if (depth == 0)
			ASSERT(block == 0);

		if (block == 0)
		{
			block = TRY(m_fs.reserve_free_block(block_group()));
			m_inode.blocks += inode_blocks_per_fs_block;

			auto block_buffer = m_fs.get_block_buffer();
			memset(block_buffer.data(), 0x00, block_buffer.size());
			TRY(m_fs.write_block(block, block_buffer));
		}

		if (depth == 0)
			return block;

		auto block_buffer = m_fs.get_block_buffer();
		TRY(m_fs.read_block(block, block_buffer));

		uint32_t divisor = 1;
		for (uint32_t i = 1; i < depth; i++)
			divisor *= indices_per_fs_block;

		uint32_t& new_block = block_buffer.span().as_span<uint32_t>()[(index / divisor) % indices_per_fs_block];

		uint32_t allocated_block = TRY(allocate_new_block_to_indirect_block(new_block, index, depth - 1));
		TRY(m_fs.write_block(block, block_buffer));

		TRY(sync());

		return allocated_block;
	}

	BAN::ErrorOr<uint32_t> Ext2Inode::allocate_new_block(uint32_t data_block_index)
	{
		const uint32_t inode_blocks_per_fs_block = blksize() / 512;
		const uint32_t indices_per_fs_block = blksize() / sizeof(uint32_t);

		if (data_block_index < 12)
		{
			ASSERT(m_inode.block[data_block_index] == 0);
			m_inode.block[data_block_index] = TRY(m_fs.reserve_free_block(block_group()));
			m_inode.blocks += inode_blocks_per_fs_block;
			TRY(sync());
			return m_inode.block[data_block_index];
		}
		data_block_index -= 12;

		if (data_block_index < indices_per_fs_block)
			return TRY(allocate_new_block_to_indirect_block(m_inode.block[12], data_block_index, 1));
		data_block_index -= indices_per_fs_block;

		if (data_block_index < indices_per_fs_block * indices_per_fs_block)
			return TRY(allocate_new_block_to_indirect_block(m_inode.block[13], data_block_index, 2));
		data_block_index -= indices_per_fs_block * indices_per_fs_block;

		if (data_block_index < indices_per_fs_block * indices_per_fs_block * indices_per_fs_block)
			return TRY(allocate_new_block_to_indirect_block(m_inode.block[14], data_block_index, 3));

		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<void> Ext2Inode::sync()
	{
		auto inode_location = TRY(m_fs.locate_inode(ino()));
		auto block_buffer = m_fs.get_block_buffer();

		TRY(m_fs.read_block(inode_location.block, block_buffer));
		if (memcmp(block_buffer.data() + inode_location.offset, &m_inode, sizeof(Ext2::Inode)))
		{
			memcpy(block_buffer.data() + inode_location.offset, &m_inode, sizeof(Ext2::Inode));
			TRY(m_fs.write_block(inode_location.block, block_buffer));
		}

		return {};
	}

	BAN::ErrorOr<BAN::RefPtr<Inode>> Ext2Inode::find_inode_impl(BAN::StringView file_name)
	{
		ASSERT(mode().ifdir());

		auto block_buffer = m_fs.get_block_buffer();

		for (uint32_t i = 0; i < max_used_data_block_count(); i++)
		{
			// FIXME: can we actually assume directories have all their blocks allocated
			const uint32_t block_index = TRY(fs_block_of_data_block_index(i)).value();
			TRY(m_fs.read_block(block_index, block_buffer));

			BAN::ConstByteSpan entry_span = block_buffer.span();
			while (entry_span.size() >= sizeof(Ext2::LinkedDirectoryEntry))
			{
				auto& entry = entry_span.as<const Ext2::LinkedDirectoryEntry>();
				BAN::StringView entry_name(entry.name, entry.name_len);
				if (entry.inode && entry_name == file_name)
					return BAN::RefPtr<Inode>(TRY(Ext2Inode::create(m_fs, entry.inode)));
				entry_span = entry_span.slice(entry.rec_len);
			}
		}

		return BAN::Error::from_errno(ENOENT);
	}

}
