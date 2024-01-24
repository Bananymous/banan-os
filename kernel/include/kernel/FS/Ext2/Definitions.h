#pragma once

#include <stdint.h>

namespace Kernel::Ext2
{

	struct Superblock
	{
		uint32_t inodes_count;
		uint32_t blocks_count;
		uint32_t r_blocks_count;
		uint32_t free_blocks_count;
		uint32_t free_inodes_count;
		uint32_t first_data_block;
		uint32_t log_block_size;
		uint32_t log_frag_size;
		uint32_t blocks_per_group;
		uint32_t frags_per_group;
		uint32_t inodes_per_group;
		uint32_t mtime;
		uint32_t wtime;
		uint16_t mnt_count;
		uint16_t max_mnt_count;
		uint16_t magic;
		uint16_t state;
		uint16_t errors;
		uint16_t minor_rev_level;
		uint32_t lastcheck;
		uint32_t checkinterval;
		uint32_t creator_os;
		uint32_t rev_level;
		uint16_t def_resuid;
		uint16_t def_resgid;

		// -- EXT2_DYNAMIC_REV Specific --
		uint8_t __extension_start[0];
		uint32_t first_ino;
		uint16_t inode_size;
		uint16_t block_group_nr;
		uint32_t feature_compat;
		uint32_t feature_incompat;
		uint32_t feature_ro_compat;
		uint8_t  uuid[16];
		uint8_t  volume_name[16];
		char     last_mounted[64];
		uint32_t algo_bitmap;

		// -- Performance Hints --
		uint8_t s_prealloc_blocks;
		uint8_t s_prealloc_dir_blocks;
		uint16_t __alignment;

		// -- Journaling Support --
		uint8_t  journal_uuid[16];
		uint32_t journal_inum;
		uint32_t journal_dev;
		uint32_t last_orphan;

		// -- Directory Indexing Support --
		uint32_t hash_seed[4];
		uint8_t  def_hash_version;
		uint8_t  __padding[3];

		// -- Other options --
		uint32_t default_mount_options;
		uint32_t first_meta_bg;
	};

	struct BlockGroupDescriptor
	{
		uint32_t block_bitmap;
		uint32_t inode_bitmap;
		uint32_t inode_table;
		uint16_t free_blocks_count;
		uint16_t free_inodes_count;
		uint16_t used_dirs_count;
		uint8_t __padding[2];
		uint8_t __reserved[12];
	};

	struct Inode
	{
		uint16_t mode;
		uint16_t uid;
		uint32_t size;
		uint32_t atime;
		uint32_t ctime;
		uint32_t mtime;
		uint32_t dtime;
		uint16_t gid;
		uint16_t links_count;
		uint32_t blocks;
		uint32_t flags;
		uint32_t osd1;
		uint32_t block[15];
		uint32_t generation;
		uint32_t file_acl;
		uint32_t dir_acl;
		uint32_t faddr;
		uint32_t osd2[3];
	};

	struct LinkedDirectoryEntry
	{
		uint32_t inode;
		uint16_t rec_len;
		uint8_t name_len;
		uint8_t file_type;
		char name[0];
	};

	namespace Enum
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

}
