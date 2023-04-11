#include <BAN/ScopeGuard.h>
#include <BAN/StringView.h>
#include <kernel/FS/Ext2.h>
#include <kernel/RTC.h>

#define EXT2_DEBUG_PRINT 0
#define VERIFY_INODE_EXISTANCE 1

namespace Kernel
{

	namespace Ext2::Enum
	{		

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

	BAN::ErrorOr<BAN::RefPtr<Inode>> Ext2Inode::create(Ext2FS& fs, uint32_t inode_inode, BAN::StringView name)
	{
		BAN::Vector<uint8_t> block_buffer;
		TRY(block_buffer.resize(fs.block_size()));

		auto inode_location = TRY(fs.locate_inode(inode_inode));
		fs.read_block(inode_location.block, block_buffer.span());
		auto& inode = *(Ext2::Inode*)(block_buffer.data() + inode_location.offset);
		Ext2Inode* result = new Ext2Inode(fs, inode, inode_inode, name);
		if (result == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<Inode>::adopt(result);
	}

	BAN::ErrorOr<uint32_t> Ext2Inode::data_block_index(uint32_t asked_data_block)
	{
		uint32_t data_blocks_count = m_inode.blocks / (2 << m_fs.superblock().log_block_size);
		uint32_t blocks_per_array = (1024 << m_fs.superblock().log_block_size) / sizeof(uint32_t);

		ASSERT(asked_data_block < data_blocks_count);

		// Direct block
		if (asked_data_block < 12)
		{
			uint32_t block = m_inode.block[asked_data_block];
			if (block == 0)
				return BAN::Error::from_errno(EIO);
			return block;
		}

		asked_data_block -= 12;

		uint32_t block_size = m_fs.block_size();
		BAN::Vector<uint8_t> block_buffer;
		TRY(block_buffer.resize(block_size));

		// Singly indirect block
		if (asked_data_block < blocks_per_array)
		{
			if (m_inode.block[12] == 0)
				return BAN::Error::from_errno(EIO);
			m_fs.read_block(m_inode.block[12], block_buffer.span()); // Block array
			uint32_t block = ((uint32_t*)block_buffer.data())[asked_data_block];
			if (block == 0)
				return BAN::Error::from_errno(EIO);
			return block;
		}

		asked_data_block -= blocks_per_array;

		// Doubly indirect blocks
		if (asked_data_block < blocks_per_array * blocks_per_array)
		{
			m_fs.read_block(m_inode.block[13], block_buffer.span()); // Singly indirect array
			uint32_t direct_block = ((uint32_t*)block_buffer.data())[asked_data_block / blocks_per_array];
			if (direct_block == 0)
				return BAN::Error::from_errno(EIO);
			m_fs.read_block(direct_block, block_buffer.span()); // Block array
			uint32_t block = ((uint32_t*)block_buffer.data())[asked_data_block % blocks_per_array];
			if (block == 0)
				return BAN::Error::from_errno(EIO);
			return block;
		}

		asked_data_block -= blocks_per_array * blocks_per_array;

		// Triply indirect blocks
		if (asked_data_block < blocks_per_array * blocks_per_array * blocks_per_array)
		{
			m_fs.read_block(m_inode.block[14], block_buffer.span()); // Doubly indirect array
			uint32_t singly_indirect_block = ((uint32_t*)block_buffer.data())[asked_data_block / (blocks_per_array * blocks_per_array)];
			if (singly_indirect_block == 0)
				return BAN::Error::from_errno(EIO);
			m_fs.read_block(singly_indirect_block, block_buffer.span()); // Singly indirect array
			uint32_t direct_block = ((uint32_t*)block_buffer.data())[(asked_data_block / blocks_per_array) % blocks_per_array];
			if (direct_block == 0)
				return BAN::Error::from_errno(EIO);
			m_fs.read_block(direct_block, block_buffer.span()); // Block array
			uint32_t block = ((uint32_t*)block_buffer.data())[asked_data_block % blocks_per_array];
			if (block == 0)
				return BAN::Error::from_errno(EIO);
			return block;
		}

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

		uint32_t block_size = 1024 << m_fs.superblock().log_block_size;
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

	BAN::ErrorOr<BAN::Vector<BAN::String>> Ext2Inode::read_directory_entries(size_t index)
	{
		if (!mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		
		uint32_t data_block_count = blocks();
		if (index >= data_block_count)
			return BAN::Vector<BAN::String>();

		uint32_t block_size = blksize();
		uint32_t block_index = TRY(data_block_index(index));

		BAN::Vector<uint8_t> block_buffer;
		TRY(block_buffer.resize(block_size));

		m_fs.read_block(block_index, block_buffer.span());

		BAN::Vector<BAN::String> entries;

		const uint8_t* block_buffer_end = block_buffer.data() + block_size;
		const uint8_t* entry_addr = block_buffer.data();
		while (entry_addr < block_buffer_end)
		{
			auto& entry = *(Ext2::LinkedDirectoryEntry*)entry_addr;
			if (entry.inode)
				TRY(entries.emplace_back(BAN::StringView(entry.name, entry.name_len)));
			entry_addr += entry.rec_len;
		}

		return entries;
	}

	BAN::ErrorOr<void> Ext2Inode::create_file(BAN::StringView name, mode_t mode)
	{
		if (!this->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);

		if (name.size() > 255)
			return BAN::Error::from_errno(ENAMETOOLONG);

		uint32_t block_size = m_fs.block_size();
		BAN::Vector<uint8_t> block_buffer;
		TRY(block_buffer.resize(block_size));

		auto error_or = read_directory_inode(name);
		if (!error_or.is_error())
			return BAN::Error::from_errno(EEXISTS);
		if (error_or.error().get_error_code() != ENOENT)
			return error_or.error();

		uint64_t current_time = BAN::to_unix_time(RTC::get_current_time());

		Ext2::Inode ext2_inode;
		ext2_inode.mode			= mode;
		ext2_inode.uid			= 0;
		ext2_inode.size			= 0;
		ext2_inode.atime 		= current_time;
		ext2_inode.ctime 		= current_time;
		ext2_inode.mtime 		= current_time;
		ext2_inode.dtime 		= current_time;
		ext2_inode.gid			= 0;
		ext2_inode.links_count	= 1;
		ext2_inode.blocks		= 0;
		ext2_inode.flags		= 0;
		ext2_inode.osd1			= 0;
		memset(ext2_inode.block, 0, sizeof(ext2_inode.block));
		ext2_inode.generation	= 0;
		ext2_inode.file_acl		= 0;
		ext2_inode.dir_acl		= 0;
		ext2_inode.faddr		= 0;
		memset(ext2_inode.osd2, 0, sizeof(ext2_inode.osd2));

		uint32_t inode_index = TRY(m_fs.create_inode(ext2_inode));

		// Insert inode to this directory
		uint32_t data_block_count = m_inode.blocks / (2 << m_fs.superblock().log_block_size);
		uint32_t block_index = TRY(data_block_index(data_block_count - 1));
		m_fs.read_block(block_index, block_buffer.span());

		const uint8_t* block_buffer_end = block_buffer.data() + block_size;
		const uint8_t* entry_addr = block_buffer.data();

		uint32_t needed_entry_len = sizeof(Ext2::LinkedDirectoryEntry) + name.size();

		bool insered = false;
		while (entry_addr < block_buffer_end)
		{
			auto& entry = *(Ext2::LinkedDirectoryEntry*)entry_addr;

			if (needed_entry_len <= entry.rec_len - entry.name_len - sizeof(Ext2::LinkedDirectoryEntry))
			{
				entry.rec_len = sizeof(Ext2::LinkedDirectoryEntry) + entry.name_len;
				if (uint32_t rem = entry.rec_len % 4)
					entry.rec_len += 4 - rem;

				auto& new_entry = *(Ext2::LinkedDirectoryEntry*)(entry_addr + entry.rec_len);
				new_entry.inode = inode_index;
				new_entry.rec_len = block_buffer_end - (uint8_t*)&new_entry;
				new_entry.name_len = name.size();
				new_entry.file_type = Ext2::Enum::REG_FILE;
				memcpy(new_entry.name, name.data(), name.size());

				m_fs.write_block(block_index, block_buffer.span());

				insered = true;
				break;
			}
			entry_addr += entry.rec_len;
		}

		// FIXME: If an entry cannot completely fit in one block, it must be pushed to the
		//        next data block and the rec_len of the previous entry properly adjusted.
		ASSERT(insered);

		return {};
	}

	BAN::ErrorOr<BAN::RefPtr<Inode>> Ext2Inode::read_directory_inode(BAN::StringView file_name)
	{
		if (!mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);

		uint32_t block_size = m_fs.block_size();
		BAN::Vector<uint8_t> block_buffer;
		TRY(block_buffer.resize(block_size));

		uint32_t data_block_count = blocks();

		for (uint32_t i = 0; i < data_block_count; i++)
		{
			uint32_t block_index = TRY(data_block_index(i));
			m_fs.read_block(block_index, block_buffer.span());

			const uint8_t* block_buffer_end = block_buffer.data() + block_size;
			const uint8_t* entry_addr = block_buffer.data();

			while (entry_addr < block_buffer_end)
			{
				const auto& entry = *(const Ext2::LinkedDirectoryEntry*)entry_addr;
				BAN::StringView entry_name(entry.name, entry.name_len);
				if (entry.inode && entry_name == file_name)
					return TRY(Ext2Inode::create(m_fs, entry.inode, entry_name));
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
		const uint32_t sector_size = m_partition.device().sector_size();
		ASSERT(1024 % sector_size == 0);

		// Read superblock from disk
		{
			BAN::Vector<uint8_t> superblock_buffer;
			TRY(superblock_buffer.resize(1024));

			uint32_t lba = 1024 / sector_size;
			uint32_t sector_count = 1024 / sector_size;

			TRY(m_partition.read_sectors(lba, sector_count, superblock_buffer.data()));

			memcpy(&m_superblock, superblock_buffer.data(), sizeof(Ext2::Superblock));
		}

		if (m_superblock.magic != 0xEF53)
			return BAN::Error::from_error_code(ErrorCode::Ext2_Invalid);

		if (m_superblock.rev_level < 1)
		{
			memset(m_superblock.__extension_start, 0, sizeof(Ext2::Superblock) - offsetof(Ext2::Superblock, Ext2::Superblock::__extension_start));
			m_superblock.first_ino = 11;
			m_superblock.inode_size = 128;
		}

		uint32_t number_of_block_groups       = BAN::Math::div_round_up(superblock().inodes_count, superblock().inodes_per_group);
		uint32_t number_of_block_groups_check = BAN::Math::div_round_up(superblock().blocks_count, superblock().blocks_per_group);
		if (number_of_block_groups != number_of_block_groups_check)
			return BAN::Error::from_error_code(ErrorCode::Ext2_Corrupted);

		ASSERT(!(m_superblock.feature_incompat & Ext2::Enum::FEATURE_INCOMPAT_COMPRESSION));
		//ASSERT(!(m_superblock.feature_incompat & Ext2::Enum::FEATURE_INCOMPAT_FILETYPE));
		ASSERT(!(m_superblock.feature_incompat & Ext2::Enum::FEATURE_INCOMPAT_JOURNAL_DEV));
		ASSERT(!(m_superblock.feature_incompat & Ext2::Enum::FEATURE_INCOMPAT_META_BG));
		ASSERT(!(m_superblock.feature_incompat & Ext2::Enum::FEATURE_INCOMPAT_RECOVER));

#if EXT2_DEBUG_PRINT
		dprintln("EXT2");
		dprintln("  inodes        {}", m_superblock.inodes_count);
		dprintln("  blocks        {}", m_superblock.blocks_count);
		dprintln("  version       {}.{}", m_superblock.rev_level, m_superblock.minor_rev_level);
		dprintln("  first data at {}", m_superblock.first_data_block);
		dprintln("  block size    {}", 1024 << m_superblock.log_block_size);
#endif

		return {};
	}

	BAN::ErrorOr<void> Ext2FS::initialize_root_inode()
	{
		m_root_inode = TRY(Ext2Inode::create(*this, Ext2::Enum::ROOT_INO, ""));

#if EXT2_DEBUG_PRINT
		dprintln("root inode:");
		dprintln("  created  {}", ext2_root_inode().ctime);
		dprintln("  modified {}", ext2_root_inode().mtime);
		dprintln("  accessed {}", ext2_root_inode().atime);
#endif
		return {};
	}

	BAN::ErrorOr<uint32_t> Ext2FS::create_inode(const Ext2::Inode& ext2_inode)
	{
		ASSERT(ext2_inode.size == 0);

		uint32_t block_size = this->block_size();
		BAN::Vector<uint8_t> bgd_buffer;
		TRY(bgd_buffer.resize(block_size));
		BAN::Vector<uint8_t> inode_bitmap;
		TRY(inode_bitmap.resize(block_size));

		uint32_t number_of_block_groups = BAN::Math::div_round_up(superblock().inodes_count, superblock().inodes_per_group);
		for (uint32_t group = 0; group < number_of_block_groups; group++)
		{
			auto bgd_location = locate_block_group_descriptior(group);
			read_block(bgd_location.block, bgd_buffer.span());

			auto& bgd = *(Ext2::BlockGroupDescriptor*)(bgd_buffer.data() + bgd_location.offset);
			if (bgd.free_inodes_count == 0)
				continue;

			read_block(bgd.inode_bitmap, inode_bitmap.span());
			for (uint32_t inode_offset = 0; inode_offset < superblock().inodes_per_group; inode_offset++)
			{
				uint32_t byte = inode_offset / 8;
				uint32_t bit  = inode_offset % 8;
				if ((inode_bitmap[byte] & (1 << bit)) == 0)
				{
					inode_bitmap[byte] |= (1 << bit);
					write_block(bgd.inode_bitmap, inode_bitmap.span());

					bgd.free_inodes_count--;
					write_block(bgd_location.block, bgd_buffer.span());

					return group * superblock().inodes_per_group + inode_offset + 1;
				}
			}
		}

		return BAN::Error::from_error_code(ErrorCode::Ext2_NoInodes);
	}

	void Ext2FS::read_block(uint32_t block, BAN::Span<uint8_t> buffer)
	{
		uint32_t sector_size = m_partition.device().sector_size();
		uint32_t block_size = this->block_size();
		uint32_t sectors_per_block = block_size / sector_size;

		ASSERT(buffer.size() >= block_size);
		
		MUST(m_partition.read_sectors(block * sectors_per_block, sectors_per_block, buffer.data()));
	}

	void Ext2FS::write_block(uint32_t block, BAN::Span<const uint8_t> data)
	{
		uint32_t sector_size = m_partition.device().sector_size();
		uint32_t block_size = this->block_size();
		uint32_t sectors_per_block = block_size / sector_size;

		ASSERT(data.size() <= block_size);
		MUST(m_partition.write_sectors(block * sectors_per_block, sectors_per_block, data.data()));
	}

	BAN::ErrorOr<Ext2FS::BlockLocation> Ext2FS::locate_inode(uint32_t inode_index)
	{
		ASSERT(inode_index < superblock().inodes_count);

		uint32_t block_size = this->block_size();
		BAN::Vector<uint8_t> bgd_buffer;
		TRY(bgd_buffer.resize(block_size));

		uint32_t inode_block_group = (inode_index - 1) / superblock().inodes_per_group;
		uint32_t local_inode_index = (inode_index - 1) % superblock().inodes_per_group;

		uint32_t inode_table_byte_offset = (local_inode_index * superblock().inode_size);

		auto bgd_location = locate_block_group_descriptior(inode_block_group);
		read_block(bgd_location.block, bgd_buffer.span());
		auto& bgd = *(Ext2::BlockGroupDescriptor*)(bgd_buffer.data() + bgd_location.offset);

		BlockLocation location;
		location.block = bgd.inode_table + inode_table_byte_offset / block_size;
		location.offset = inode_table_byte_offset % block_size;

#if VERIFY_INODE_EXISTANCE
		// Note we reuse the bgd_buffer since it is not needed anymore
		ASSERT(superblock().inodes_per_group <= block_size * 8);
		read_block(bgd.inode_bitmap, bgd_buffer.span());
		uint32_t byte = local_inode_index / 8;
		uint32_t bit  = local_inode_index % 8;
		ASSERT(bgd_buffer[byte] & (1 << bit));
#endif

		return location;
	}

	Ext2FS::BlockLocation Ext2FS::locate_block_group_descriptior(uint32_t group_index)
	{
		uint32_t block_size = this->block_size();

		uint32_t block_group_count = BAN::Math::div_round_up(superblock().inodes_count, superblock().inodes_per_group);
		ASSERT(group_index < block_group_count);

		uint32_t bgd_byte_offset = sizeof(Ext2::BlockGroupDescriptor) * group_index;
		uint32_t bgd_table_block = superblock().first_data_block + (bgd_byte_offset / block_size) + 1;

		BlockLocation location;
		location.block  = bgd_table_block + (bgd_byte_offset / block_size);
		location.offset =					(bgd_byte_offset % block_size);
		return location;
	}

}