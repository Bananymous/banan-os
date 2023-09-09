#include <BAN/Function.h>
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

	BAN::ErrorOr<BAN::RefPtr<Inode>> Ext2Inode::create(Ext2FS& fs, uint32_t inode_ino)
	{
		auto inode_location = TRY(fs.locate_inode(inode_ino));

		BAN::Vector<uint8_t> block_buffer;
		TRY(block_buffer.resize(fs.block_size()));

		fs.read_block(inode_location.block, block_buffer.span());

		auto& inode = *(Ext2::Inode*)(block_buffer.data() + inode_location.offset);

		Ext2Inode* result = new Ext2Inode(fs, inode, inode_ino);
		if (result == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<Inode>::adopt(result);
	}

#define READ_INDIRECT(block, container)										\
	if (block)																\
		m_fs.read_block(block, block_buffer.span());						\
	else																	\
	{																		\
		if (!allocate)														\
			return BAN::Error::from_error_code(ErrorCode::Ext2_Corrupted);	\
		memset(block_buffer.data(), 0, block_size);							\
		block = TRY(m_fs.reserve_free_block(block_group()));				\
		m_fs.write_block(container, block_buffer.span());					\
	}

#define READ_INDIRECT_TOP(block)											\
	if (block)																\
		m_fs.read_block(block, block_buffer.span());						\
	else																	\
	{																		\
		if (!allocate)														\
			return BAN::Error::from_error_code(ErrorCode::Ext2_Corrupted);	\
		memset(block_buffer.data(), 0, block_size);							\
		block = TRY(m_fs.reserve_free_block(block_group()));				\
	}

	BAN::ErrorOr<void> Ext2Inode::for_data_block_index(uint32_t asked_data_block, const BAN::Function<void(uint32_t&)>& callback, bool allocate)
	{
		const uint32_t block_size = blksize();
		const uint32_t data_blocks_count = blocks();
		const uint32_t blocks_per_array = block_size / sizeof(uint32_t);

		ASSERT(asked_data_block < data_blocks_count);

		// Direct block
		if (asked_data_block < 12)
		{
			uint32_t& block = m_inode.block[asked_data_block];
			uint32_t block_copy = block;
			callback(block);

			if (block != block_copy)
				TRY(sync());

			return {};
		}

		asked_data_block -= 12;

		BAN::Vector<uint8_t> block_buffer;
		TRY(block_buffer.resize(block_size));

		// Singly indirect block
		if (asked_data_block < blocks_per_array)
		{
			READ_INDIRECT_TOP(m_inode.block[12]);

			uint32_t& block = ((uint32_t*)block_buffer.data())[asked_data_block];
			uint32_t block_copy = block;
			callback(block);

			if (block != block_copy)
				m_fs.write_block(m_inode.block[12], block_buffer.span());

			return {};
		}

		asked_data_block -= blocks_per_array;

		// Doubly indirect blocks
		if (asked_data_block < blocks_per_array * blocks_per_array)
		{
			READ_INDIRECT_TOP(m_inode.block[13]);

			uint32_t& direct_block = ((uint32_t*)block_buffer.data())[asked_data_block / blocks_per_array];
			READ_INDIRECT(direct_block, m_inode.block[13]);

			uint32_t& block = ((uint32_t*)block_buffer.data())[asked_data_block % blocks_per_array];
			uint32_t block_copy = block;
			callback(block);

			if (block != block_copy)
				m_fs.write_block(direct_block, block_buffer.span());

			return {};
		}

		asked_data_block -= blocks_per_array * blocks_per_array;

		// Triply indirect blocks
		if (asked_data_block < blocks_per_array * blocks_per_array * blocks_per_array)
		{
			READ_INDIRECT_TOP(m_inode.block[14]);

			uint32_t& doubly_indirect_block = ((uint32_t*)block_buffer.data())[asked_data_block / (blocks_per_array * blocks_per_array)];
			READ_INDIRECT(doubly_indirect_block, m_inode.block[14]);

			uint32_t& singly_direct_block = ((uint32_t*)block_buffer.data())[(asked_data_block / blocks_per_array) % blocks_per_array];
			READ_INDIRECT(singly_direct_block, doubly_indirect_block);

			uint32_t& block = ((uint32_t*)block_buffer.data())[asked_data_block % blocks_per_array];
			uint32_t block_copy = block;
			callback(block);

			if (block != block_copy)
				m_fs.write_block(singly_direct_block, block_buffer.span());

			return {};
		}

		ASSERT_NOT_REACHED();
	}

#undef READ_INDIRECT
#undef READ_INDIRECT_TOP

	BAN::ErrorOr<uint32_t> Ext2Inode::data_block_index(uint32_t asked_data_block)
	{
		uint32_t result;
		TRY(for_data_block_index(asked_data_block, [&result] (uint32_t& index) { result = index; }, false));
		ASSERT(result != 0);
		return result;
	}

	BAN::ErrorOr<BAN::String> Ext2Inode::link_target_impl()
	{
		ASSERT(mode().iflnk());
		if (m_inode.size < sizeof(m_inode.block))
			return BAN::String((const char*)m_inode.block);
		return BAN::Error::from_errno(ENOTSUP);
	}

	BAN::ErrorOr<size_t> Ext2Inode::read_impl(off_t offset, void* buffer, size_t count)
	{
		// FIXME: update atime if needed

		ASSERT(!mode().ifdir());
		ASSERT(offset >= 0);

		if (offset >= UINT32_MAX || count >= UINT32_MAX || offset + count >= UINT32_MAX)
			return BAN::Error::from_errno(EOVERFLOW);

		if (offset >= m_inode.size)
			return 0;
		if (offset + count > m_inode.size)
			count = m_inode.size - offset;

		const uint32_t block_size = blksize();

		BAN::Vector<uint8_t> block_buffer;
		TRY(block_buffer.resize(block_size));

		const uint32_t first_block = offset / block_size;
		const uint32_t last_block = BAN::Math::div_round_up<uint32_t>(offset + count, block_size);

		size_t n_read = 0;

		for (uint32_t block = first_block; block < last_block; block++)
		{
			uint32_t block_index = TRY(data_block_index(block));
			m_fs.read_block(block_index, block_buffer.span());

			uint32_t copy_offset = (offset + n_read) % block_size;
			uint32_t to_copy = BAN::Math::min<uint32_t>(block_size - copy_offset, count - n_read);
			memcpy((uint8_t*)buffer + n_read, block_buffer.data() + copy_offset, to_copy);

			n_read += to_copy;
		}

		return n_read;
	}

	BAN::ErrorOr<size_t> Ext2Inode::write_impl(off_t offset, const void* buffer, size_t count)
	{
		// FIXME: update atime if needed

		ASSERT(!mode().ifdir());
		ASSERT(offset >= 0);

		if (offset >= UINT32_MAX || count >= UINT32_MAX || offset + count >= UINT32_MAX)
			return BAN::Error::from_errno(EOVERFLOW);

		if (m_inode.size < offset + count)
			TRY(truncate_impl(offset + count));

		const uint32_t block_size = blksize();

		BAN::Vector<uint8_t> block_buffer;
		TRY(block_buffer.resize(block_size));

		const uint8_t* u8buffer = (const uint8_t*)buffer;

		size_t written = 0;

		// Write partial block
		if (offset % block_size)
		{
			uint32_t block_index = offset / block_size;
			uint32_t block_offset = offset % block_size;

			uint32_t data_block_index = TRY(this->data_block_index(block_index));
			uint32_t to_copy = BAN::Math::min<uint32_t>(block_size - block_offset, written);

			m_fs.read_block(data_block_index, block_buffer.span());
			memcpy(block_buffer.data() + block_offset, buffer, to_copy);
			m_fs.write_block(data_block_index, block_buffer.span());

			u8buffer += to_copy;
			offset += to_copy;
			written -= to_copy;
		}

		while (written >= block_size)
		{
			uint32_t data_block_index = TRY(this->data_block_index(offset / block_size));

			m_fs.write_block(data_block_index, BAN::Span<const uint8_t>(u8buffer, block_size));

			u8buffer += block_size;
			offset += block_size;
			written -= block_size;
		}

		if (written > 0)
		{
			uint32_t data_block_index = TRY(this->data_block_index(offset / block_size));

			m_fs.read_block(data_block_index, block_buffer.span());
			memcpy(block_buffer.data(), u8buffer, written);
			m_fs.write_block(data_block_index, block_buffer.span());
		}

		return count;
	}

	BAN::ErrorOr<void> Ext2Inode::truncate_impl(size_t new_size)
	{
		if (m_inode.size == new_size)
			return {};

		const uint32_t block_size = blksize();
		const uint32_t current_data_blocks = blocks();
		const uint32_t needed_data_blocks = BAN::Math::div_round_up<uint32_t>(new_size, block_size);

		if (new_size < m_inode.size)
		{
			m_inode.size = new_size;
			TRY(sync());
			return {};
		}

		BAN::Vector<uint8_t> block_buffer;
		TRY(block_buffer.resize(block_size));

		if (uint32_t rem = m_inode.size % block_size)
		{
			uint32_t last_block_index = TRY(data_block_index(current_data_blocks - 1));

			m_fs.read_block(last_block_index, block_buffer.span());
			memset(block_buffer.data() + rem, 0, block_size - rem);
			m_fs.write_block(last_block_index, block_buffer.span());
		}

		memset(block_buffer.data(), 0, block_size);
		while (blocks() < needed_data_blocks)
		{
			uint32_t block_index = TRY(allocate_new_block());
			m_fs.write_block(block_index, block_buffer.span());
		}

		m_inode.size = new_size;
		TRY(sync());

		return {};
	}

	BAN::ErrorOr<void> Ext2Inode::list_next_inodes_impl(off_t offset, DirectoryEntryList* list, size_t list_size)
	{
		ASSERT(mode().ifdir());
		ASSERT(offset >= 0);

		const uint32_t data_block_count = blocks();
		if (offset >= data_block_count)
		{
			list->entry_count = 0;
			return {};
		}

		const uint32_t block_size = blksize();
		const uint32_t block_index = TRY(data_block_index(offset));

		BAN::Vector<uint8_t> block_buffer;
		TRY(block_buffer.resize(block_size));

		m_fs.read_block(block_index, block_buffer.span());

		// First determine if we have big enough list
		{
			const uint8_t* block_buffer_end = block_buffer.data() + block_size;
			const uint8_t* entry_addr = block_buffer.data();

			size_t needed_size = sizeof(DirectoryEntryList);
			while (entry_addr < block_buffer_end)
			{
				auto& entry = *(Ext2::LinkedDirectoryEntry*)entry_addr;
				if (entry.inode)
					needed_size += sizeof(DirectoryEntry) + entry.name_len + 1;
				entry_addr += entry.rec_len;
			}

			if (needed_size > list_size)
				return BAN::Error::from_errno(EINVAL);
		}

		// Second fill the list
		{
			DirectoryEntry* ptr = list->array;
			list->entry_count = 0;

			const uint8_t* block_buffer_end = block_buffer.data() + block_size;
			const uint8_t* entry_addr = block_buffer.data();
			while (entry_addr < block_buffer_end)
			{
				auto& entry = *(Ext2::LinkedDirectoryEntry*)entry_addr;
				if (entry.inode)
				{
					ptr->dirent.d_ino = entry.inode;
					ptr->rec_len = sizeof(DirectoryEntry) + entry.name_len + 1;
					memcpy(ptr->dirent.d_name, entry.name, entry.name_len);
					ptr->dirent.d_name[entry.name_len] = '\0';

					ptr = ptr->next();
					list->entry_count++;
				}
				entry_addr += entry.rec_len;
			}
		}

		return {};
	}

	BAN::ErrorOr<void> Ext2Inode::create_file_impl(BAN::StringView name, mode_t mode, uid_t uid, gid_t gid)
	{
		if (!this->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);

		if (name.size() > 255)
			return BAN::Error::from_errno(ENAMETOOLONG);

		if (!(Mode(mode).ifreg()))
			return BAN::Error::from_errno(EINVAL);

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

		timespec current_time = SystemTimer::get().real_time();

		Ext2::Inode ext2_inode
		{
			.mode			= (uint16_t)mode,
			.uid			= (uint16_t)uid,
			.size			= 0,
			.atime 			= (uint32_t)current_time.tv_sec,
			.ctime 			= (uint32_t)current_time.tv_sec,
			.mtime 			= (uint32_t)current_time.tv_sec,
			.dtime 			= 0,
			.gid			= (uint16_t)gid,
			.links_count	= 1,
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

		const uint32_t inode_index = TRY(m_fs.create_inode(ext2_inode));

		const uint32_t block_size = m_fs.block_size();
		BAN::Vector<uint8_t> block_buffer;
		TRY(block_buffer.resize(block_size));

		auto write_inode = [&](uint32_t entry_offset, uint32_t entry_rec_len)
		{
			auto typed_mode = Mode(mode);
			uint8_t file_type = (m_fs.superblock().rev_level == Ext2::Enum::GOOD_OLD_REV) ? 0
				: typed_mode.ifreg()  ? Ext2::Enum::REG_FILE
				: typed_mode.ifdir()  ? Ext2::Enum::DIR
				: typed_mode.ifchr()  ? Ext2::Enum::CHRDEV
				: typed_mode.ifblk()  ? Ext2::Enum::BLKDEV
				: typed_mode.ififo()  ? Ext2::Enum::FIFO
				: typed_mode.ifsock() ? Ext2::Enum::SOCK
				: typed_mode.iflnk()  ? Ext2::Enum::SYMLINK
				: 0;

			auto& new_entry = *(Ext2::LinkedDirectoryEntry*)(block_buffer.data() + entry_offset);
			new_entry.inode = inode_index;
			new_entry.rec_len = entry_rec_len;
			new_entry.name_len = name.size();
			new_entry.file_type = file_type;
			memcpy(new_entry.name, name.data(), name.size());
		};

		uint32_t block_index = 0;
		uint32_t entry_offset = 0;

		uint32_t needed_entry_len = sizeof(Ext2::LinkedDirectoryEntry) + name.size();
		if (auto rem = needed_entry_len % 4)
			needed_entry_len += 4 - rem;

		const uint32_t data_block_count = blocks();
		if (data_block_count == 0)
			goto needs_new_block;

		// Try to insert inode to last data block
		block_index = TRY(data_block_index(data_block_count - 1));
		m_fs.read_block(block_index, block_buffer.span());

		while (entry_offset < block_size)
		{
			auto& entry = *(Ext2::LinkedDirectoryEntry*)(block_buffer.data() + entry_offset);

			uint32_t entry_min_rec_len = sizeof(Ext2::LinkedDirectoryEntry) + entry.name_len;
			if (auto rem = entry_min_rec_len % 4)
				entry_min_rec_len += 4 - rem;

			if (entry.inode == 0 && needed_entry_len <= entry.rec_len)
			{
				write_inode(entry_offset, entry.rec_len);
				m_fs.write_block(block_index, block_buffer.span());
				return {};
			}
			else if (needed_entry_len <= entry.rec_len - entry_min_rec_len)
			{
				uint32_t new_rec_len = entry.rec_len - entry_min_rec_len;
				entry.rec_len = entry_min_rec_len;

				write_inode(entry_offset + entry.rec_len, new_rec_len);
				m_fs.write_block(block_index, block_buffer.span());
				return {};
			}

			entry_offset += entry.rec_len;
		}

needs_new_block:
		block_index = TRY(allocate_new_block());

		m_fs.read_block(block_index, block_buffer.span());
		write_inode(0, block_size);
		m_fs.write_block(block_index, block_buffer.span());

		return {};
	}

	BAN::ErrorOr<uint32_t> Ext2Inode::allocate_new_block()
	{
		uint32_t new_block_index = TRY(m_fs.reserve_free_block(block_group()));
		auto set_index_func = [new_block_index] (uint32_t& index) { index = new_block_index; };

		const uint32_t blocks_per_data_block = blksize() / 512;

		m_inode.blocks += blocks_per_data_block;
		if (auto res = for_data_block_index(blocks() - 1, set_index_func, true); res.is_error())
		{
			m_inode.blocks -= blocks_per_data_block;
			return res.release_error();
		}

		if (mode().ifdir())
			m_inode.size += blksize();

		TRY(sync());
		return new_block_index;
	}

	BAN::ErrorOr<void> Ext2Inode::sync()
	{
		auto inode_location_or_error = m_fs.locate_inode(ino());
		if (inode_location_or_error.is_error())
		{
			dwarnln("Open inode not found from filesystem");
			return BAN::Error::from_error_code(ErrorCode::Ext2_Corrupted);
		}

		auto inode_location = inode_location_or_error.release_value();

		const uint32_t block_size = blksize();

		BAN::Vector<uint8_t> block_buffer;
		TRY(block_buffer.resize(block_size));

		m_fs.read_block(inode_location.block, block_buffer.span());
		if (memcmp(block_buffer.data() + inode_location.offset, &m_inode, sizeof(Ext2::Inode)))
		{
			memcpy(block_buffer.data() + inode_location.offset, &m_inode, sizeof(Ext2::Inode));
			m_fs.write_block(inode_location.block, block_buffer.span());
		}

		return {};
	}

	BAN::ErrorOr<BAN::RefPtr<Inode>> Ext2Inode::find_inode_impl(BAN::StringView file_name)
	{
		ASSERT(mode().ifdir());

		const uint32_t block_size = blksize();
		const uint32_t data_block_count = blocks();

		BAN::Vector<uint8_t> block_buffer;
		TRY(block_buffer.resize(block_size));

		for (uint32_t i = 0; i < data_block_count; i++)
		{
			const uint32_t block_index = TRY(data_block_index(i));
			m_fs.read_block(block_index, block_buffer.span());

			const uint8_t* block_buffer_end = block_buffer.data() + block_size;
			const uint8_t* entry_addr = block_buffer.data();

			while (entry_addr < block_buffer_end)
			{
				const auto& entry = *(const Ext2::LinkedDirectoryEntry*)entry_addr;
				BAN::StringView entry_name(entry.name, entry.name_len);
				if (entry.inode && entry_name == file_name)
					return TRY(Ext2Inode::create(m_fs, entry.inode));
				entry_addr += entry.rec_len;
			}
		}

		return BAN::Error::from_errno(ENOENT);
	}

}