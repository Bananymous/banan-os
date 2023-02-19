#include <BAN/ScopeGuard.h>
#include <BAN/StringView.h>
#include <kernel/FS/Ext2.h>

#include <kernel/kprint.h>

#define EXT2_DEBUG_PRINT 1

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
			IFSOKC = 0xC000,
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

	}

	bool Ext2Inode::is_directory() const
	{
		return m_inode.mode & Ext2::Enum::IFDIR;
	}

	bool Ext2Inode::is_regular_file() const
	{
		return m_inode.mode & Ext2::Enum::IFREG;
	}
	
	BAN::ErrorOr<BAN::Vector<uint8_t>> Ext2Inode::read_all() const
	{
		return BAN::Error::from_string("not implemented");
	}

	BAN::ErrorOr<BAN::RefCounted<Inode>> Ext2Inode::directory_find(BAN::StringView name) const
	{
		if (!is_directory())
			return BAN::Error::from_string("Inode is not a directory");

		uint32_t data_block_count = m_inode.blocks / (2 << m_fs->superblock().log_block_size);
		uint32_t data_blocks_found = 0;

		for (uint32_t data_block = 0; data_block < 12 && data_blocks_found < data_block_count; data_block++)
		{
			if (m_inode.block[0] == 0)
				continue;
			data_blocks_found++;

			auto inode_data = TRY(m_fs->read_block(m_inode.block[data_block]));
			uintptr_t inode_data_end = (uintptr_t)inode_data.data() + inode_data.size();
			
			uintptr_t entry_addr = (uintptr_t)inode_data.data();
			while (entry_addr < inode_data_end)
			{
				Ext2::LinkedDirectoryEntry* entry = (Ext2::LinkedDirectoryEntry*)entry_addr;
				
				BAN::StringView entry_name = BAN::StringView(entry->name, entry->name_len);
				if (entry->inode && name == entry_name)
				{
					Ext2::Inode asked_inode = TRY(m_fs->read_inode(entry->inode));
					return BAN::RefCounted<Inode>(new Ext2Inode(m_fs, BAN::move(asked_inode), entry_name));
				}

				entry_addr += entry->rec_len;
			}
		}

		return BAN::Error::from_string("Could not find the asked inode");
	}

	BAN::ErrorOr<BAN::Vector<BAN::RefCounted<Inode>>> Ext2Inode::directory_inodes() const
	{
		if (!is_directory())
			return BAN::Error::from_string("Inode is not a directory");

		uint32_t data_block_count = m_inode.blocks / (2 << m_fs->superblock().log_block_size);
		uint32_t data_blocks_found = 0;

		BAN::Vector<BAN::RefCounted<Inode>> inodes;

		// FIXME: implement indirect pointers
		for (uint32_t data_block = 0; data_block < 12 && data_blocks_found < data_block_count; data_block++)
		{
			if (m_inode.block[0] == 0)
				continue;
			data_blocks_found++;

			auto inode_data = TRY(m_fs->read_block(m_inode.block[data_block]));
			uintptr_t inode_data_end = (uintptr_t)inode_data.data() + inode_data.size();
			
			uintptr_t entry_addr = (uintptr_t)inode_data.data();
			while (entry_addr < inode_data_end)
			{
				Ext2::LinkedDirectoryEntry* entry = (Ext2::LinkedDirectoryEntry*)entry_addr;
				
				if (entry->inode)
				{
					BAN::StringView entry_name = BAN::StringView(entry->name, entry->name_len);
					Ext2::Inode current_inode = TRY(m_fs->read_inode(entry->inode));
					auto ref_counted_inode = BAN::RefCounted<Inode>(new Ext2Inode(m_fs, BAN::move(current_inode), entry_name));
					TRY(inodes.push_back(BAN::move(ref_counted_inode)));
				}

				entry_addr += entry->rec_len;
			}
		}

		// FIXME: for now we can just assert that we found everything in direct pointers
		ASSERT(data_blocks_found == data_block_count);

		return inodes;
	}

	BAN::ErrorOr<Ext2FS*> Ext2FS::create(DiskDevice::Partition& partition)
	{
		Ext2FS* ext2fs = new Ext2FS(partition);
		if (ext2fs == nullptr)
			return BAN::Error::from_string("Could not allocate Ext2FS");
		TRY(ext2fs->initialize_superblock());
		TRY(ext2fs->initialize_block_group_descriptors());
		TRY(ext2fs->initialize_root_inode());
		return ext2fs;
	}

	BAN::ErrorOr<void> Ext2FS::initialize_superblock()
	{
		const uint32_t sector_size = m_partition.device().sector_size();
		ASSERT(1024 % sector_size == 0);

		// Read superblock from disk
		{
			uint8_t* superblock_buffer = (uint8_t*)kmalloc(1024);
			if (superblock_buffer == nullptr)
				return BAN::Error::from_string("Could not allocate memory for superblocks");
			BAN::ScopeGuard _([superblock_buffer] { kfree(superblock_buffer); });

			uint32_t lba = 1024 / sector_size;
			uint32_t sector_count = 1024 / sector_size;

			if (!m_partition.read_sectors(lba, sector_count, superblock_buffer))
				return BAN::Error::from_string("Could not read from partition");

			memcpy(&m_superblock, superblock_buffer, sizeof(Ext2::Superblock));
		}

		if (m_superblock.magic != 0xEF53)
			return BAN::Error::from_string("Not a ext2 filesystem");

		if (m_superblock.rev_level < 1)
		{
			memset(m_superblock.__extension_start, 0, sizeof(Ext2::Superblock) - offsetof(Ext2::Superblock, Ext2::Superblock::__extension_start));
			m_superblock.first_ino = 11;
			m_superblock.inode_size = 128;
		}

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

	BAN::ErrorOr<void> Ext2FS::initialize_block_group_descriptors()
	{
		const uint32_t sector_size = m_partition.device().sector_size();
		const uint32_t block_size = 1024 << m_superblock.log_block_size;
		const uint32_t sectors_per_block = block_size / sector_size;
		ASSERT(block_size % sector_size == 0);

		uint32_t number_of_block_groups       = BAN::Math::div_round_up(m_superblock.inodes_count, m_superblock.inodes_per_group);
		uint32_t number_of_block_groups_check = BAN::Math::div_round_up(m_superblock.blocks_count, m_superblock.blocks_per_group);
		if (number_of_block_groups != number_of_block_groups_check)
			return BAN::Error::from_string("Ambiguous number of blocks");

		uint32_t block_group_descriptor_table_block = m_superblock.first_data_block + 1;
		uint32_t block_group_descriptor_table_sector_count = BAN::Math::div_round_up(32u * number_of_block_groups, sector_size);

		uint8_t* block_group_descriptor_table_buffer = (uint8_t*)kmalloc(block_group_descriptor_table_sector_count * sector_size);
		if (block_group_descriptor_table_buffer == nullptr)
			return BAN::Error::from_string("Could not allocate memory for block group descriptor table");
		BAN::ScopeGuard _([block_group_descriptor_table_buffer] { kfree(block_group_descriptor_table_buffer); });

		if (!m_partition.read_sectors(
			block_group_descriptor_table_block * sectors_per_block,
			block_group_descriptor_table_sector_count,
			block_group_descriptor_table_buffer
		))
			return BAN::Error::from_string("Could not read from partition");

		TRY(m_block_group_descriptors.resize(number_of_block_groups));

		for (uint32_t i = 0; i < number_of_block_groups; i++)
		{
			memcpy(&m_block_group_descriptors[i], block_group_descriptor_table_buffer + 32u * i, sizeof(Ext2::BlockGroupDescriptor));

#if EXT2_DEBUG_PRINT
			dprintln("block group descriptor {}", i);
			dprintln("  block bitmap   {}", m_block_group_descriptors[i].block_bitmap);
			dprintln("  inode bitmap   {}", m_block_group_descriptors[i].inode_bitmap);
			dprintln("  inode table    {}", m_block_group_descriptors[i].inode_table);
			dprintln("  unalloc blocks {}", m_block_group_descriptors[i].free_blocks_count);
			dprintln("  unalloc inodes {}", m_block_group_descriptors[i].free_inodes_count);
#endif
		}

		return {};
	}

	BAN::ErrorOr<void> Ext2FS::initialize_root_inode()
	{
		m_root_inode = BAN::RefCounted<Inode>(new Ext2Inode(this, TRY(read_inode(Ext2::Enum::ROOT_INO)), ""));
#if EXT2_DEBUG_PRINT
		dprintln("root inode:");
		dprintln("  created  {}", ext2_root_inode().ctime);
		dprintln("  modified {}", ext2_root_inode().mtime);
		dprintln("  accessed {}", ext2_root_inode().atime);
#endif
		return {};
	}

	BAN::ErrorOr<Ext2::Inode> Ext2FS::read_inode(uint32_t inode)
	{
		uint32_t block_size = 1024 << m_superblock.log_block_size;

		uint32_t inode_block_group = (inode - 1) / m_superblock.inodes_per_group;
		uint32_t local_inode_index = (inode - 1) % m_superblock.inodes_per_group;

		uint32_t inode_table_offset_blocks = (local_inode_index * m_superblock.inode_size) / block_size;
		uint32_t inode_block_offset = (local_inode_index * m_superblock.inode_size) % block_size;

		uint32_t inode_block = m_block_group_descriptors[inode_block_group].inode_table + inode_table_offset_blocks;

		auto inode_block_buffer = TRY(read_block(inode_block));
		Ext2::Inode ext2_inode;
		memcpy(&ext2_inode, inode_block_buffer.data() + inode_block_offset, sizeof(Ext2::Inode));
		return ext2_inode;
	}

	BAN::ErrorOr<BAN::Vector<uint8_t>> Ext2FS::read_block(uint32_t block)
	{
		const uint32_t sector_size = m_partition.device().sector_size();
		uint32_t block_size = 1024 << m_superblock.log_block_size;
		ASSERT(block_size % sector_size == 0);
		uint32_t sectors_per_block = block_size / sector_size;
		
		BAN::Vector<uint8_t> block_buffer;
		TRY(block_buffer.resize(block_size));

		if (!m_partition.read_sectors(block * sectors_per_block, sectors_per_block, block_buffer.data()))
			return BAN::Error::from_string("Could not read from partition");
		
		return block_buffer;
	}

	const Ext2::Inode& Ext2FS::ext2_root_inode() const
	{
		return reinterpret_cast<const Ext2Inode*>(m_root_inode.operator->())->m_inode;
	}

}