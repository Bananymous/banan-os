#pragma once

#include <BAN/String.h>
#include <BAN/StringView.h>
#include <kernel/Storage/StorageDevice.h>
#include <kernel/FS/FileSystem.h>

namespace Kernel
{

	namespace Ext2
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

	}

	class Ext2FS;

	class Ext2Inode : public Inode
	{
	public:
		virtual uid_t uid() const override { return m_inode.uid; }
		virtual gid_t gid() const override { return m_inode.gid; }
		virtual size_t size() const override { return m_inode.size; }

		virtual mode_t mode() const override { return m_inode.mode; }

		virtual BAN::StringView name() const override { return m_name; }

		virtual BAN::ErrorOr<size_t> read(size_t, void*, size_t) override;

		virtual BAN::ErrorOr<void> create_file(BAN::StringView, mode_t) override;

		virtual Type type() const override { return Type::Ext2; }
		virtual bool operator==(const Inode& other) const override;

	protected:
		virtual BAN::ErrorOr<BAN::Vector<BAN::RefPtr<Inode>>> directory_inodes_impl() override;
		virtual BAN::ErrorOr<BAN::RefPtr<Inode>> directory_find_impl(BAN::StringView) override;

	private:
		BAN::ErrorOr<uint32_t> data_block_index(uint32_t);

		uint32_t index() const { return m_index; }

	private:
		Ext2Inode(Ext2FS& fs, Ext2::Inode inode, BAN::StringView name, uint32_t index)
			: m_fs(fs)
			, m_inode(inode) 
			, m_name(name)
			, m_index(index)
		{}
		static BAN::ErrorOr<BAN::RefPtr<Inode>> create(Ext2FS&, uint32_t, BAN::StringView);

	private:
		Ext2FS& m_fs;
		Ext2::Inode m_inode;
		BAN::String m_name;
		uint32_t m_index;

		friend class Ext2FS;
		friend class BAN::RefPtr<Ext2Inode>;
	};

	class Ext2FS : public FileSystem
	{
	public:	
		static BAN::ErrorOr<Ext2FS*> create(StorageDevice::Partition&);

		virtual BAN::RefPtr<Inode> root_inode() override { return m_root_inode; }

	private:
		Ext2FS(StorageDevice::Partition& partition)
			: m_partition(partition)
		{}

		BAN::ErrorOr<void> initialize_superblock();
		BAN::ErrorOr<void> initialize_root_inode();

		BAN::ErrorOr<uint32_t> create_inode(const Ext2::Inode&);
		BAN::ErrorOr<void> delete_inode(uint32_t);
		BAN::ErrorOr<void> resize_inode(uint32_t, size_t);

		BAN::ErrorOr<BAN::Vector<uint8_t>> read_block(uint32_t);
		BAN::ErrorOr<void> write_block(uint32_t, BAN::Span<const uint8_t>);

		const Ext2::Superblock& superblock() const { return m_superblock; }

		struct BlockLocation
		{
			uint32_t block;
			uint32_t offset;
		};
		BAN::ErrorOr<BlockLocation> locate_inode(uint32_t);
		BlockLocation locate_block_group_descriptior(uint32_t);

		uint32_t block_size() const { return 1024 << superblock().log_block_size; }

	private:
		StorageDevice::Partition& m_partition;

		BAN::RefPtr<Inode> m_root_inode;

		Ext2::Superblock m_superblock;

		friend class Ext2Inode;
	};

}