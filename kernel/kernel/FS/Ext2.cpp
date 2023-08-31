#include <BAN/ScopeGuard.h>
#include <BAN/StringView.h>
#include <kernel/FS/Ext2.h>
#include <kernel/Timer/Timer.h>

#define EXT2_DEBUG_PRINT 0
#define VERIFY_INODE_EXISTANCE 1

namespace Kernel
{

	namespace Ext2::Enum
	{

		constexpr uint16_t SUPER_MAGIC = 0xEF53;

		enum State
		{
			VALID_FS = 1,
			ERROR_FS = 2,
		};

		enum Errors
		{
			ERRORS_CONTINUE = 1,
			ERRORS_RO = 2,
			ERRORS_PANIC = 3,
		};

		enum CreatorOS
		{
			OS_LINUX = 0,
			OS_HURD = 1,
			OS_MASIX = 2,
			OS_FREEBSD = 3,
			OS_LITES = 4,
		};

		enum RevLevel
		{
			GOOD_OLD_REV = 0,
			DYNAMIC_REV = 1,
		};

		enum Rev0Constant
		{
			GOOD_OLD_FIRST_INO = 11,
			GOOD_OLD_INODE_SIZE = 128,
		};

		enum FeatureCompat
		{
			FEATURE_COMPAT_DIR_PREALLOC		= 0x0001,
			FEATURE_COMPAT_IMAGIC_INODES	= 0x0002,
			FEATURE_COMPAT_HAS_JOURNAL		= 0x0004,
			FEATURE_COMPAT_EXT_ATTR			= 0x0008,
			FEATURE_COMPAT_RESIZE_INO		= 0x0010,
			FEATURE_COMPAT_DIR_INDEX		= 0x0020,
		};

		enum FeaturesIncompat
		{
			FEATURE_INCOMPAT_COMPRESSION	= 0x0001,
			FEATURE_INCOMPAT_FILETYPE		= 0x0002,
			FEATURE_INCOMPAT_RECOVER		= 0x0004,
			FEATURE_INCOMPAT_JOURNAL_DEV	= 0x0008,
			FEATURE_INCOMPAT_META_BG		= 0x0010,
		};

		enum FeaturesRoCompat
		{
			FEATURE_RO_COMPAT_SPARSE_SUPER	= 0x0001,
			FEATURE_RO_COMPAT_LARGE_FILE	= 0x0002,
			FEATURE_RO_COMPAT_BTREE_DIR		= 0x0004,
		};

		enum AlgoBitmap
		{
			LZV1_ALG	= 0,
			LZRW3A_ALG	= 1,
			GZIP_ALG	= 2,
			BZIP2_ALG	= 3,
			LZO_ALG		= 4,
		};

		enum ReservedInodes
		{
			BAD_INO = 1,
			ROOT_INO = 2,
			ACL_IDX_INO = 3,
			ACL_DATA_INO = 4,
			BOOT_LOADER_INO = 5,
			UNDEL_DIR_INO = 6,
		};

		enum InodeMode
		{
			// -- file format --
			IFSOCK = 0xC000,
			IFLNK = 0xA000,
			IFREG = 0x8000,
			IFBLK = 0x6000,
			IFDIR = 0x4000,
			IFCHR = 0x2000,
			IFIFO = 0x1000,

			// -- process execution user/group override --
			ISUID = 0x0800,
			ISGID = 0x0400,
			ISVTX = 0x0200,

			// -- access rights --
			IRUSR = 0x0100,
			IWUSR = 0x0080,
			IXUSR = 0x0040,
			IRGRP = 0x0020,
			IWGRP = 0x0010,
			IXGRP = 0x0008,
			IROTH = 0x0004,
			IWOTH = 0x0002,
			IXOTH = 0x0001,
		};

		enum InodeFlags
		{
			SECRM_FL		= 0x00000001,
			UNRM_FL			= 0x00000002,
			COMPR_FL		= 0x00000004,
			SYNC_FL			= 0x00000008,
			IMMUTABLE_FL	= 0x00000010,
			APPEND_FL		= 0x00000020,
			NODUMP_FL		= 0x00000040,
			NOATIME_FL		= 0x00000080,
			// -- Reserved for compression usage --
			DIRTY_FL		= 0x00000100,
			COMPRBLK_FL		= 0x00000200,
			NOCOMPR_FL		= 0x00000400,
			ECOMPR_FL		= 0x00000800,
			// -- End of compression flags --
			BTREE_FL		= 0x00001000,
			INDEX_FL		= 0x00001000,
			IMAGIC_FL		= 0x00002000,
			JOURNAL_DATA_FL	= 0x00004000,
			RESERVED_FL		= 0x80000000,
		};

		enum FileType
		{
			UNKNOWN = 0,
			REG_FILE = 1,
			DIR = 2,
			CHRDEV = 3,
			BLKDEV = 4,
			FIFO = 5,
			SOCK = 6,
			SYMLINK = 7,
		};

	}

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

	BAN::ErrorOr<BAN::String> Ext2Inode::link_target()
	{
		ASSERT(mode().iflnk());
		if (m_inode.size < sizeof(m_inode.block))
			return BAN::String((const char*)m_inode.block);
		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<size_t> Ext2Inode::read(size_t offset, void* buffer, size_t count)
	{
		// FIXME: update atime if needed

		if (mode().ifdir())
			return BAN::Error::from_errno(EISDIR);

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

	BAN::ErrorOr<size_t> Ext2Inode::write(size_t offset, const void* buffer, size_t count)
	{
		if (offset >= UINT32_MAX || count == UINT32_MAX || offset + count >= UINT32_MAX)
			return BAN::Error::from_errno(EOVERFLOW);

		if (mode().ifdir())
			return BAN::Error::from_errno(EISDIR);

		if (m_inode.size < offset + count)
			TRY(truncate(offset + count));

		const uint32_t block_size = blksize();

		BAN::Vector<uint8_t> block_buffer;
		TRY(block_buffer.resize(block_size));

		const uint8_t* u8buffer = (const uint8_t*)buffer;

		// Write partial block
		if (offset % block_size)
		{
			uint32_t block_index = offset / block_size;
			uint32_t block_offset = offset % block_size;

			uint32_t data_block_index = TRY(this->data_block_index(block_index));
			uint32_t to_copy = BAN::Math::min<uint32_t>(block_size - block_offset, count);

			m_fs.read_block(data_block_index, block_buffer.span());
			memcpy(block_buffer.data() + block_offset, buffer, to_copy);
			m_fs.write_block(data_block_index, block_buffer.span());

			u8buffer += to_copy;
			offset += to_copy;
			count -= to_copy;
		}

		while (count >= block_size)
		{
			uint32_t data_block_index = TRY(this->data_block_index(offset / block_size));

			m_fs.write_block(data_block_index, BAN::Span<const uint8_t>(u8buffer, block_size));

			u8buffer += block_size;
			offset += block_size;
			count -= block_size;
		}

		if (count > 0)
		{
			uint32_t data_block_index = TRY(this->data_block_index(offset / block_size));

			m_fs.read_block(data_block_index, block_buffer.span());
			memcpy(block_buffer.data(), u8buffer, count);
			m_fs.write_block(data_block_index, block_buffer.span());
		}

		return count;
	}

	BAN::ErrorOr<void> Ext2Inode::truncate(size_t new_size)
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

	BAN::ErrorOr<void> Ext2Inode::directory_read_next_entries(off_t offset, DirectoryEntryList* list, size_t list_size)
	{
		if (!mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);

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

	BAN::ErrorOr<void> Ext2Inode::create_file(BAN::StringView name, mode_t mode, uid_t uid, gid_t gid)
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

		auto error_or = directory_find_inode(name);
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
			auto typed_mode = Mode { mode };
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
		ASSERT_NOT_REACHED();

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

	BAN::ErrorOr<BAN::RefPtr<Inode>> Ext2Inode::directory_find_inode(BAN::StringView file_name)
	{
		if (!mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);

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

	BAN::ErrorOr<Ext2FS*> Ext2FS::create(Partition& partition)
	{
		Ext2FS* ext2fs = new Ext2FS(partition);
		if (ext2fs == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		BAN::ScopeGuard guard([ext2fs] { delete ext2fs; });
		TRY(ext2fs->initialize_superblock());
		TRY(ext2fs->initialize_root_inode());
		guard.disable();
		return ext2fs;
	}

	BAN::ErrorOr<void> Ext2FS::initialize_superblock()
	{
		// Read superblock from disk
		{
			const uint32_t sector_size = m_partition.device().sector_size();
			ASSERT(1024 % sector_size == 0);

			const uint32_t lba = 1024 / sector_size;
			const uint32_t sector_count = BAN::Math::div_round_up<uint32_t>(sizeof(Ext2::Superblock), sector_size);

			BAN::Vector<uint8_t> superblock_buffer;
			TRY(superblock_buffer.resize(sector_count * sector_size));

			TRY(m_partition.read_sectors(lba, sector_count, superblock_buffer.data()));

			memcpy(&m_superblock, superblock_buffer.data(), sizeof(Ext2::Superblock));
		}

		if (m_superblock.magic != Ext2::Enum::SUPER_MAGIC)
			return BAN::Error::from_error_code(ErrorCode::Ext2_Invalid);

		if (m_superblock.rev_level == Ext2::Enum::GOOD_OLD_REV)
		{
			memset(m_superblock.__extension_start, 0, sizeof(Ext2::Superblock) - offsetof(Ext2::Superblock, Ext2::Superblock::__extension_start));
			m_superblock.first_ino = Ext2::Enum::GOOD_OLD_FIRST_INO;
			m_superblock.inode_size = Ext2::Enum::GOOD_OLD_INODE_SIZE;
		}

		const uint32_t number_of_block_groups       = BAN::Math::div_round_up(superblock().inodes_count, superblock().inodes_per_group);
		const uint32_t number_of_block_groups_check = BAN::Math::div_round_up(superblock().blocks_count, superblock().blocks_per_group);
		if (number_of_block_groups != number_of_block_groups_check)
			return BAN::Error::from_error_code(ErrorCode::Ext2_Corrupted);

		if (!(m_superblock.feature_incompat & Ext2::Enum::FEATURE_INCOMPAT_FILETYPE))
		{
			dwarnln("Directory entries without filetype not supported");
			return BAN::Error::from_errno(ENOTSUP);
		}
		if (m_superblock.feature_incompat & Ext2::Enum::FEATURE_INCOMPAT_COMPRESSION)
		{
			dwarnln("Required FEATURE_INCOMPAT_COMPRESSION");
			return BAN::Error::from_errno(ENOTSUP);
		}
		if (m_superblock.feature_incompat & Ext2::Enum::FEATURE_INCOMPAT_JOURNAL_DEV)
		{
			dwarnln("Required FEATURE_INCOMPAT_JOURNAL_DEV");
			return BAN::Error::from_errno(ENOTSUP);
		}
		if (m_superblock.feature_incompat & Ext2::Enum::FEATURE_INCOMPAT_META_BG)
		{
			dwarnln("Required FEATURE_INCOMPAT_META_BG");
			return BAN::Error::from_errno(ENOTSUP);
		}
		if (m_superblock.feature_incompat & Ext2::Enum::FEATURE_INCOMPAT_RECOVER)
		{
			dwarnln("Required FEATURE_INCOMPAT_RECOVER");
			return BAN::Error::from_errno(ENOTSUP);
		}

#if EXT2_DEBUG_PRINT
		dprintln("EXT2");
		dprintln("  inodes        {}", m_superblock.inodes_count);
		dprintln("  blocks        {}", m_superblock.blocks_count);
		dprintln("  version       {}.{}", m_superblock.rev_level, m_superblock.minor_rev_level);
		dprintln("  first data at {}", m_superblock.first_data_block);
		dprintln("  block size    {}", 1024 << m_superblock.log_block_size);
		dprintln("  inode size    {}", m_superblock.inode_size);
#endif

		{
			BAN::Vector<uint8_t> block_buffer;
			TRY(block_buffer.resize(block_size()));

			if (superblock().rev_level == Ext2::Enum::GOOD_OLD_REV)
			{
				// In revision 0 all blockgroups contain superblock backup
				TRY(m_superblock_backups.reserve(number_of_block_groups - 1));
				for (uint32_t i = 1; i < number_of_block_groups; i++)
					MUST(block_buffer.push_back(i));
			}
			else
			{
				// In other revision superblock backups are on blocks 1 and powers of 3, 5 and 7
				TRY(m_superblock_backups.push_back(1));
				for (uint32_t i = 3; i < number_of_block_groups; i *= 3)
					TRY(m_superblock_backups.push_back(i));
				for (uint32_t i = 5; i < number_of_block_groups; i *= 5)
					TRY(m_superblock_backups.push_back(i));
				for (uint32_t i = 7; i < number_of_block_groups; i *= 7)
					TRY(m_superblock_backups.push_back(i));

				// We don't really care if this succeeds or not
				(void)m_superblock_backups.shrink_to_fit();
			}

			for (uint32_t bg : m_superblock_backups)
			{
				read_block(superblock().first_data_block + superblock().blocks_per_group * bg, block_buffer.span());
				Ext2::Superblock& superblock_backup = *(Ext2::Superblock*)block_buffer.data();
				if (superblock_backup.magic != Ext2::Enum::SUPER_MAGIC)
					derrorln("superblock backup at block {} is invalid ({4H})", bg, superblock_backup.magic);
			}
		}

		return {};
	}

	BAN::ErrorOr<void> Ext2FS::initialize_root_inode()
	{
		m_root_inode = TRY(Ext2Inode::create(*this, Ext2::Enum::ROOT_INO));

		//(void)locate_inode(2271);

#if EXT2_DEBUG_PRINT
		dprintln("root inode:");
		dprintln("  created  {}", root_inode()->ctime().tv_sec);
		dprintln("  modified {}", root_inode()->mtime().tv_sec);
		dprintln("  accessed {}", root_inode()->atime().tv_sec);
#endif
		return {};
	}

	BAN::ErrorOr<uint32_t> Ext2FS::create_inode(const Ext2::Inode& ext2_inode)
	{
		ASSERT(ext2_inode.size == 0);

		if (m_superblock.free_inodes_count == 0)
			return BAN::Error::from_errno(ENOSPC);

		const uint32_t block_size = this->block_size();

		BAN::Vector<uint8_t> bgd_buffer;
		TRY(bgd_buffer.resize(block_size));

		BAN::Vector<uint8_t> inode_bitmap;
		TRY(inode_bitmap.resize(block_size));

		uint32_t current_group = -1;
		BlockLocation bgd_location {};
		Ext2::BlockGroupDescriptor* bgd = nullptr;

		for (uint32_t ino = superblock().first_ino; ino <= superblock().inodes_count; ino++)
		{
			const uint32_t ino_group = (ino - 1) / superblock().inodes_per_group;
			const uint32_t ino_index = (ino - 1) % superblock().inodes_per_group;

			if (ino_group != current_group)
			{
				current_group = ino_group;

				bgd_location = locate_block_group_descriptior(current_group);
				read_block(bgd_location.block, bgd_buffer.span());

				bgd = (Ext2::BlockGroupDescriptor*)(bgd_buffer.data() + bgd_location.offset);
				if (bgd->free_inodes_count == 0)
				{
					ino = (current_group + 1) * superblock().inodes_per_group;
					continue;
				}

				read_block(bgd->inode_bitmap, inode_bitmap.span());
			}

			const uint32_t ino_bitmap_byte = ino_index / 8;
			const uint32_t ino_bitmap_bit  = ino_index % 8;
			if (inode_bitmap[ino_bitmap_byte] & (1 << ino_bitmap_bit))
				continue;

			inode_bitmap[ino_bitmap_byte] |= 1 << ino_bitmap_bit;
			write_block(bgd->inode_bitmap, inode_bitmap.span());

			bgd->free_inodes_count--;
			write_block(bgd_location.block, bgd_buffer.span());

			const uint32_t inode_table_offset = ino_index * superblock().inode_size;
			const BlockLocation inode_location
			{
				.block  = inode_table_offset / block_size + bgd->inode_table,
				.offset = inode_table_offset % block_size
			};

			// NOTE: we don't need inode bitmap anymore, so we can reuse it
			auto& inode_buffer = inode_bitmap;

			read_block(inode_location.block, inode_buffer.span());
			memcpy(inode_buffer.data() + inode_location.offset, &ext2_inode, sizeof(Ext2::Inode));
			if (superblock().inode_size > sizeof(Ext2::Inode))
				memset(inode_buffer.data() + inode_location.offset + sizeof(Ext2::Inode), 0, superblock().inode_size - sizeof(Ext2::Inode));
			write_block(inode_location.block, inode_buffer.span());

			m_superblock.free_inodes_count--;
			sync_superblock();

			return ino;
		}

		derrorln("Corrupted file system. Superblock indicates free inodes but none were found.");
		return BAN::Error::from_error_code(ErrorCode::Ext2_Corrupted);
	}

	void Ext2FS::read_block(uint32_t block, BAN::Span<uint8_t> buffer)
	{
		const uint32_t sector_size = m_partition.device().sector_size();
		const uint32_t block_size = this->block_size();
		const uint32_t sectors_per_block = block_size / sector_size;
		const uint32_t sectors_before = 2048 / sector_size;

		ASSERT(block >= 2);
		ASSERT(buffer.size() >= block_size);
		MUST(m_partition.read_sectors(sectors_before + (block - 2) * sectors_per_block, sectors_per_block, buffer.data()));
	}

	void Ext2FS::write_block(uint32_t block, BAN::Span<const uint8_t> buffer)
	{
		const uint32_t sector_size = m_partition.device().sector_size();
		const uint32_t block_size = this->block_size();
		const uint32_t sectors_per_block = block_size / sector_size;
		const uint32_t sectors_before = 2048 / sector_size;

		ASSERT(block >= 2);
		ASSERT(buffer.size() >= block_size);
		MUST(m_partition.write_sectors(sectors_before + (block - 2) * sectors_per_block, sectors_per_block, buffer.data()));
	}

	void Ext2FS::sync_superblock()
	{
		const uint32_t sector_size = m_partition.device().sector_size();
		ASSERT(1024 % sector_size == 0);

		const uint32_t superblock_bytes =
			(m_superblock.rev_level == Ext2::Enum::GOOD_OLD_REV)
				? offsetof(Ext2::Superblock, __extension_start)
				: sizeof(Ext2::Superblock);

		const uint32_t lba = 1024 / sector_size;
		const uint32_t sector_count = BAN::Math::div_round_up<uint32_t>(superblock_bytes, sector_size);

		BAN::Vector<uint8_t> superblock_buffer;
		MUST(superblock_buffer.resize(sector_count * sector_size));

		MUST(m_partition.read_sectors(lba, sector_count, superblock_buffer.data()));
		if (memcmp(superblock_buffer.data(), &m_superblock, superblock_bytes))
		{
			memcpy(superblock_buffer.data(), &m_superblock, superblock_bytes);
			MUST(m_partition.write_sectors(lba, sector_count, superblock_buffer.data()));
		}
	}

	BAN::ErrorOr<uint32_t> Ext2FS::reserve_free_block(uint32_t primary_bgd)
	{
		if (m_superblock.r_blocks_count >= m_superblock.free_blocks_count)
			return BAN::Error::from_errno(ENOSPC);

		const uint32_t block_size = this->block_size();

		BAN::Vector<uint8_t> bgd_buffer;
		TRY(bgd_buffer.resize(block_size));

		BAN::Vector<uint8_t> block_bitmap;
		TRY(block_bitmap.resize(block_size));

		auto check_block_group =
			[&](uint32_t block_group) -> uint32_t
			{
				auto bgd_location = locate_block_group_descriptior(block_group);
				read_block(bgd_location.block, bgd_buffer.span());

				auto& bgd = *(Ext2::BlockGroupDescriptor*)(bgd_buffer.data() + bgd_location.offset);
				if (bgd.free_blocks_count == 0)
					return 0;

				read_block(bgd.block_bitmap, block_bitmap.span());
				for (uint32_t block_offset = 0; block_offset < m_superblock.blocks_per_group; block_offset++)
				{
					uint32_t byte = block_offset / 8;
					uint32_t bit  = block_offset % 8;
					if (block_bitmap[byte] & (1 << bit))
						continue;

					block_bitmap[byte] |= 1 << bit;
					write_block(bgd.block_bitmap, block_bitmap.span());

					bgd.free_blocks_count--;
					write_block(bgd_location.block, bgd_buffer.span());

					m_superblock.free_blocks_count--;
					sync_superblock();

					return m_superblock.first_data_block + m_superblock.blocks_per_group * block_group + block_offset;
				}

				derrorln("Corrupted file system. Block group descriptor indicates free blocks but none were found");
				return 0;
			};

		if (auto ret = check_block_group(primary_bgd))
			return ret;

		uint32_t number_of_block_groups = BAN::Math::div_round_up(m_superblock.blocks_count, m_superblock.blocks_per_group);
		for (uint32_t block_group = 0; block_group < number_of_block_groups; block_group++)
			if (block_group != primary_bgd)
				if (auto ret = check_block_group(block_group))
					return ret;

		derrorln("Corrupted file system. Superblock indicates free blocks but none were found.");
		return BAN::Error::from_error_code(ErrorCode::Ext2_Corrupted);
	}

	BAN::ErrorOr<Ext2FS::BlockLocation> Ext2FS::locate_inode(uint32_t ino)
	{
		ASSERT(ino < superblock().inodes_count);

		const uint32_t block_size = this->block_size();

		BAN::Vector<uint8_t> bgd_buffer;
		TRY(bgd_buffer.resize(block_size));

		const uint32_t inode_group = (ino - 1) / superblock().inodes_per_group;
		const uint32_t inode_index = (ino - 1) % superblock().inodes_per_group;

		auto bgd_location = locate_block_group_descriptior(inode_group);

		read_block(bgd_location.block, bgd_buffer.span());

		auto& bgd = *(Ext2::BlockGroupDescriptor*)(bgd_buffer.data() + bgd_location.offset);

		const uint32_t inode_byte_offset = inode_index * superblock().inode_size;
		BlockLocation location
		{
			.block  = inode_byte_offset / block_size + bgd.inode_table,
			.offset = inode_byte_offset % block_size
		};

#if VERIFY_INODE_EXISTANCE
		const uint32_t inode_bitmap_block = bgd.inode_bitmap;

		// NOTE: we can reuse the bgd_buffer since it is not needed anymore
		auto& inode_bitmap = bgd_buffer;

		read_block(inode_bitmap_block, inode_bitmap.span());

		const uint32_t byte = inode_index / 8;
		const uint32_t bit  = inode_index % 8;
		ASSERT(inode_bitmap[byte] & (1 << bit));
#endif

		return location;
	}

	Ext2FS::BlockLocation Ext2FS::locate_block_group_descriptior(uint32_t group_index)
	{
		const uint32_t block_size = this->block_size();

		const uint32_t block_group_count = BAN::Math::div_round_up(superblock().inodes_count, superblock().inodes_per_group);
		ASSERT(group_index < block_group_count);

		// Block Group Descriptor table is always after the superblock
		// Superblock begins at byte 1024 and is exactly 1024 bytes wide
		const uint32_t bgd_byte_offset = 2048 + sizeof(Ext2::BlockGroupDescriptor) * group_index;

		return
		{
			.block  = bgd_byte_offset / block_size,
			.offset = bgd_byte_offset % block_size
		};
	}

}