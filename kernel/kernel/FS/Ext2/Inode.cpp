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

		auto inode_location = fs.locate_inode(inode_ino);

		auto block_buffer = fs.get_block_buffer();
		fs.read_block(inode_location.block, block_buffer);

		auto& inode = *(Ext2::Inode*)(block_buffer.data() + inode_location.offset);

		Ext2Inode* result_ptr = new Ext2Inode(fs, inode, inode_ino);
		if (result_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto result = BAN::RefPtr<Ext2Inode>::adopt(result_ptr);
		TRY(fs.inode_cache().insert(inode_ino, result));
		return result;
	}

	Ext2Inode::~Ext2Inode()
	{
		if (m_inode.links_count == 0)
			cleanup_from_fs();
	}

#define VERIFY_AND_READ_BLOCK(expr) do { const uint32_t block_index = expr; ASSERT(block_index); m_fs.read_block(block_index, block_buffer); } while (false)
#define VERIFY_AND_RETURN(expr) ({ const uint32_t result = expr; ASSERT(result); return result; })

	uint32_t Ext2Inode::fs_block_of_data_block_index(uint32_t data_block_index)
	{
		ASSERT(data_block_index < blocks());

		const uint32_t indices_per_block = blksize() / sizeof(uint32_t);

		// Direct block
		if (data_block_index < 12)
			VERIFY_AND_RETURN(m_inode.block[data_block_index]);

		data_block_index -= 12;

		auto block_buffer = m_fs.get_block_buffer();

		// Singly indirect block
		if (data_block_index < indices_per_block)
		{
			VERIFY_AND_READ_BLOCK(m_inode.block[12]);
			VERIFY_AND_RETURN(((uint32_t*)block_buffer.data())[data_block_index]);
		}

		data_block_index -= indices_per_block;

		// Doubly indirect blocks
		if (data_block_index < indices_per_block * indices_per_block)
		{
			VERIFY_AND_READ_BLOCK(m_inode.block[13]);
			VERIFY_AND_READ_BLOCK(((uint32_t*)block_buffer.data())[data_block_index / indices_per_block]);
			VERIFY_AND_RETURN(((uint32_t*)block_buffer.data())[data_block_index % indices_per_block]);
		}

		data_block_index -= indices_per_block * indices_per_block;

		// Triply indirect blocks
		if (data_block_index < indices_per_block * indices_per_block * indices_per_block)
		{
			VERIFY_AND_READ_BLOCK(m_inode.block[14]);
			VERIFY_AND_READ_BLOCK(((uint32_t*)block_buffer.data())[data_block_index / (indices_per_block * indices_per_block)]);
			VERIFY_AND_READ_BLOCK(((uint32_t*)block_buffer.data())[(data_block_index / indices_per_block) % indices_per_block]);
			VERIFY_AND_RETURN(((uint32_t*)block_buffer.data())[data_block_index % indices_per_block]);
		}

		ASSERT_NOT_REACHED();
	}

#undef VERIFY_AND_READ_BLOCK
#undef VERIFY_AND_RETURN

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

		if (offset >= UINT32_MAX || buffer.size() >= UINT32_MAX || buffer.size() >= (size_t)(UINT32_MAX - offset))
			return BAN::Error::from_errno(EOVERFLOW);

		if (offset >= m_inode.size)
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
			uint32_t block_index = fs_block_of_data_block_index(data_block_index);
			m_fs.read_block(block_index, block_buffer);

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

		if (offset >= UINT32_MAX || buffer.size() >= UINT32_MAX || buffer.size() >= (size_t)(UINT32_MAX - offset))
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
			uint32_t block_index = fs_block_of_data_block_index(offset / block_size);
			uint32_t block_offset = offset % block_size;

			uint32_t to_copy = BAN::Math::min<uint32_t>(block_size - block_offset, to_write);

			m_fs.read_block(block_index, block_buffer);
			memcpy(block_buffer.data() + block_offset, buffer.data(), to_copy);
			m_fs.write_block(block_index, block_buffer);

			written += to_copy;
			offset += to_copy;
			to_write -= to_copy;
		}

		while (to_write >= block_size)
		{
			uint32_t block_index = fs_block_of_data_block_index(offset / block_size);

			memcpy(block_buffer.data(), buffer.data() + written, block_buffer.size());
			m_fs.write_block(block_index, block_buffer);

			written += block_size;
			offset += block_size;
			to_write -= block_size;
		}

		if (to_write > 0)
		{
			uint32_t block_index = fs_block_of_data_block_index(offset / block_size);

			m_fs.read_block(block_index, block_buffer);
			memcpy(block_buffer.data(), buffer.data() + written, to_write);
			m_fs.write_block(block_index, block_buffer);
		}

		return buffer.size();
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
			sync();
			return {};
		}

		auto block_buffer = m_fs.get_block_buffer();

		if (uint32_t rem = m_inode.size % block_size)
		{
			uint32_t last_block_index = fs_block_of_data_block_index(current_data_blocks - 1);

			m_fs.read_block(last_block_index, block_buffer);
			memset(block_buffer.data() + rem, 0, block_size - rem);
			m_fs.write_block(last_block_index, block_buffer);
		}

		memset(block_buffer.data(), 0, block_size);
		while (blocks() < needed_data_blocks)
		{
			uint32_t block_index = TRY(allocate_new_block());
			m_fs.write_block(block_index, block_buffer);
		}

		m_inode.size = new_size;
		sync();

		return {};
	}

	BAN::ErrorOr<void> Ext2Inode::chmod_impl(mode_t mode)
	{
		ASSERT((mode & Inode::Mode::TYPE_MASK) == 0);
		if (m_inode.mode == mode)
			return {};
		m_inode.mode = (m_inode.mode & Inode::Mode::TYPE_MASK) | mode;
		sync();
		return {};
	}

	void Ext2Inode::cleanup_from_fs()
	{
		ASSERT(m_inode.links_count == 0);
		for (uint32_t i = 0; i < blocks(); i++)
			m_fs.release_block(fs_block_of_data_block_index(i));
		m_fs.delete_inode(ino());
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
		const uint32_t block_index = fs_block_of_data_block_index(offset);

		auto block_buffer = m_fs.get_block_buffer();

		m_fs.read_block(block_index, block_buffer);

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
					ptr->dirent.d_type = entry.file_type;
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
			m_fs.delete_inode(new_ino);
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
			m_fs.delete_inode(new_ino);
			return inode_or_error.release_error();
		}

		auto inode = inode_or_error.release_value();
		BAN::ScopeGuard cleanup([&] { inode->cleanup_from_fs(); });

		TRY(inode->link_inode_to_directory(*inode, "."sv));
		TRY(inode->link_inode_to_directory(*this, ".."sv));
		
		TRY(link_inode_to_directory(*inode, name));

		cleanup.disable();

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

		auto write_inode = [&](uint32_t entry_offset, uint32_t entry_rec_len)
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

			auto& new_entry = *(Ext2::LinkedDirectoryEntry*)(block_buffer.data() + entry_offset);
			new_entry.inode = inode.ino();
			new_entry.rec_len = entry_rec_len;
			new_entry.name_len = name.size();
			new_entry.file_type = file_type;
			memcpy(new_entry.name, name.data(), name.size());

			inode.m_inode.links_count++;
			inode.sync();
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
		block_index = fs_block_of_data_block_index(data_block_count - 1);
		m_fs.read_block(block_index, block_buffer);

		while (entry_offset < block_size)
		{
			auto& entry = *(Ext2::LinkedDirectoryEntry*)(block_buffer.data() + entry_offset);

			uint32_t entry_min_rec_len = sizeof(Ext2::LinkedDirectoryEntry) + entry.name_len;
			if (auto rem = entry_min_rec_len % 4)
				entry_min_rec_len += 4 - rem;

			if (entry.inode == 0 && needed_entry_len <= entry.rec_len)
			{
				write_inode(entry_offset, entry.rec_len);
				m_fs.write_block(block_index, block_buffer);
				return {};
			}
			else if (needed_entry_len <= entry.rec_len - entry_min_rec_len)
			{
				uint32_t new_rec_len = entry.rec_len - entry_min_rec_len;
				entry.rec_len = entry_min_rec_len;

				write_inode(entry_offset + entry.rec_len, new_rec_len);
				m_fs.write_block(block_index, block_buffer);
				return {};
			}

			entry_offset += entry.rec_len;
		}

needs_new_block:
		block_index = TRY(allocate_new_block());

		m_fs.read_block(block_index, block_buffer);
		write_inode(0, block_size);
		m_fs.write_block(block_index, block_buffer);

		return {};
	}

	BAN::ErrorOr<bool> Ext2Inode::is_directory_empty()
	{
		ASSERT(mode().ifdir());
		if (m_inode.flags & Ext2::Enum::INDEX_FL)
		{
			dwarnln("deletion of indexed directory is not supported");
			return BAN::Error::from_errno(ENOTSUP);
		}

		auto block_buffer = m_fs.get_block_buffer();

		// Confirm that this doesn't contain anything else than '.' or '..'
		for (uint32_t i = 0; i < blocks(); i++)
		{
			const uint32_t block = fs_block_of_data_block_index(i);
			m_fs.read_block(block, block_buffer);

			blksize_t offset = 0;
			while (offset < blksize())
			{
				auto& entry = block_buffer.span().slice(offset).as<Ext2::LinkedDirectoryEntry>();

				if (entry.inode)
				{
					BAN::StringView entry_name(entry.name, entry.name_len);
					if (entry_name != "."sv && entry_name != ".."sv)
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

		auto block_buffer = m_fs.get_block_buffer();

		for (uint32_t i = 0; i < blocks(); i++)
		{
			const uint32_t block = fs_block_of_data_block_index(i);
			m_fs.read_block(block, block_buffer);

			bool modified = false;

			blksize_t offset = 0;
			while (offset < blksize())
			{
				auto& entry = block_buffer.span().slice(offset).as<Ext2::LinkedDirectoryEntry>();

				if (entry.inode)
				{
					BAN::StringView entry_name(entry.name, entry.name_len);

					if (entry_name == "."sv)
					{
						m_inode.links_count--;
						sync();
					}
					else if (entry_name == ".."sv)
					{
						auto parent = TRY(Ext2Inode::create(m_fs, entry.inode));
						parent->m_inode.links_count--;
						parent->sync();
					}
					else
						ASSERT_NOT_REACHED();

					modified = true;
					entry.inode = 0;
				}

				offset += entry.rec_len;
			}

			if (modified)
				m_fs.write_block(block, block_buffer);
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

		for (uint32_t i = 0; i < blocks(); i++)
		{
			const uint32_t block = fs_block_of_data_block_index(i);
			m_fs.read_block(block, block_buffer);

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

					sync();

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
					m_fs.write_block(block, block_buffer);					
				}
				offset += entry.rec_len;
			}
		}

		return {};
	}

#define READ_OR_ALLOCATE_BASE_BLOCK(index_)											\
	do {																			\
		if (m_inode.block[index_] != 0)												\
			m_fs.read_block(m_inode.block[index_], block_buffer);					\
		else																		\
		{																			\
			m_inode.block[index_] = TRY(m_fs.reserve_free_block(block_group()));	\
			memset(block_buffer.data(), 0x00, block_buffer.size());					\
		}																			\
	} while (false)

#define READ_OR_ALLOCATE_INDIRECT_BLOCK(result_, buffer_index_, parent_block_)		\
	uint32_t result_ = ((uint32_t*)block_buffer.data())[buffer_index_];				\
	if (result_ != 0)																\
		m_fs.read_block(result_, block_buffer);										\
	else																			\
	{																				\
		const uint32_t new_block_ = TRY(m_fs.reserve_free_block(block_group()));	\
																					\
		((uint32_t*)block_buffer.data())[buffer_index_] = new_block_;				\
		m_fs.write_block(parent_block_, block_buffer);								\
																					\
		result_ = new_block_;														\
		memset(block_buffer.data(), 0x00, block_buffer.size());						\
	}																				\
	do {} while (false)

#define WRITE_BLOCK_AND_RETURN(buffer_index_, parent_block_)					\
	do {																		\
		const uint32_t block_ = TRY(m_fs.reserve_free_block(block_group()));	\
																				\
		ASSERT(((uint32_t*)block_buffer.data())[buffer_index_] == 0);			\
		((uint32_t*)block_buffer.data())[buffer_index_] = block_;				\
		m_fs.write_block(parent_block_, block_buffer);							\
																				\
		m_inode.blocks += blocks_per_fs_block;									\
		update_and_sync();														\
																				\
		return block_;															\
	} while (false)

	BAN::ErrorOr<uint32_t> Ext2Inode::allocate_new_block()
	{
		const uint32_t blocks_per_fs_block = blksize() / 512;
		const uint32_t indices_per_fs_block = blksize() / sizeof(uint32_t);

		uint32_t block_array_index = blocks();

		auto update_and_sync =
			[&]
			{
				if (mode().ifdir())
					m_inode.size += blksize();
				sync();
			};

		// direct block
		if (block_array_index < 12)
		{
			const uint32_t block = TRY(m_fs.reserve_free_block(block_group()));

			ASSERT(m_inode.block[block_array_index] == 0);
			m_inode.block[block_array_index] = block;

			m_inode.blocks += blocks_per_fs_block;
			update_and_sync();
			return block;
		}

		block_array_index -= 12;

		auto block_buffer = m_fs.get_block_buffer();

		// singly indirect block
		if (block_array_index < indices_per_fs_block)
		{
			READ_OR_ALLOCATE_BASE_BLOCK(12);
			WRITE_BLOCK_AND_RETURN(block_array_index, m_inode.block[12]);
		}

		block_array_index -= indices_per_fs_block;

		// doubly indirect block
		if (block_array_index < indices_per_fs_block * indices_per_fs_block)
		{
			READ_OR_ALLOCATE_BASE_BLOCK(13);
			READ_OR_ALLOCATE_INDIRECT_BLOCK(direct_block, block_array_index / indices_per_fs_block, m_inode.block[13]);
			WRITE_BLOCK_AND_RETURN(block_array_index % indices_per_fs_block, direct_block);
		}

		block_array_index -= indices_per_fs_block * indices_per_fs_block;

		// triply indirect block
		if (block_array_index < indices_per_fs_block * indices_per_fs_block * indices_per_fs_block)
		{
			dwarnln("here");
			READ_OR_ALLOCATE_BASE_BLOCK(14);
			READ_OR_ALLOCATE_INDIRECT_BLOCK(indirect_block, block_array_index / (indices_per_fs_block * indices_per_fs_block), 14);
			READ_OR_ALLOCATE_INDIRECT_BLOCK(direct_block, (block_array_index / indices_per_fs_block) % indices_per_fs_block, indirect_block);
			WRITE_BLOCK_AND_RETURN(block_array_index % indices_per_fs_block, direct_block);
		}

		ASSERT_NOT_REACHED();
	}

#undef READ_OR_ALLOCATE_BASE_BLOCK
#undef READ_OR_ALLOCATE_INDIRECT_BLOCK
#undef WRITE_BLOCK_AND_RETURN

	void Ext2Inode::sync()
	{
		auto inode_location = m_fs.locate_inode(ino());
		auto block_buffer = m_fs.get_block_buffer();

		m_fs.read_block(inode_location.block, block_buffer);
		if (memcmp(block_buffer.data() + inode_location.offset, &m_inode, sizeof(Ext2::Inode)))
		{
			memcpy(block_buffer.data() + inode_location.offset, &m_inode, sizeof(Ext2::Inode));
			m_fs.write_block(inode_location.block, block_buffer);
		}
	}

	BAN::ErrorOr<BAN::RefPtr<Inode>> Ext2Inode::find_inode_impl(BAN::StringView file_name)
	{
		ASSERT(mode().ifdir());

		const uint32_t block_size = blksize();
		const uint32_t data_block_count = blocks();

		auto block_buffer = m_fs.get_block_buffer();

		for (uint32_t i = 0; i < data_block_count; i++)
		{
			const uint32_t block_index = fs_block_of_data_block_index(i);
			m_fs.read_block(block_index, block_buffer);

			const uint8_t* block_buffer_end = block_buffer.data() + block_size;
			const uint8_t* entry_addr = block_buffer.data();

			while (entry_addr < block_buffer_end)
			{
				const auto& entry = *(const Ext2::LinkedDirectoryEntry*)entry_addr;
				BAN::StringView entry_name(entry.name, entry.name_len);
				if (entry.inode && entry_name == file_name)
					return BAN::RefPtr<Inode>(TRY(Ext2Inode::create(m_fs, entry.inode)));
				entry_addr += entry.rec_len;
			}
		}

		return BAN::Error::from_errno(ENOENT);
	}

}