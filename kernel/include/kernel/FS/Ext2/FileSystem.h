#pragma once

#include <kernel/Storage/StorageDevice.h>
#include <kernel/FS/FileSystem.h>
#include <kernel/FS/Ext2/Inode.h>

namespace Kernel
{

	class Ext2FS final : public FileSystem
	{
	public:	
		static BAN::ErrorOr<Ext2FS*> create(Partition&);

		virtual BAN::RefPtr<Inode> root_inode() override { return m_root_inode; }

	private:
		Ext2FS(Partition& partition)
			: m_partition(partition)
		{}

		BAN::ErrorOr<void> initialize_superblock();
		BAN::ErrorOr<void> initialize_root_inode();

		BAN::ErrorOr<uint32_t> create_inode(const Ext2::Inode&);
		BAN::ErrorOr<void> delete_inode(uint32_t);
		BAN::ErrorOr<void> resize_inode(uint32_t, size_t);

		void read_block(uint32_t, BAN::Span<uint8_t>);
		void write_block(uint32_t, BAN::Span<const uint8_t>);
		void sync_superblock();

		BAN::ErrorOr<uint32_t> reserve_free_block(uint32_t primary_bgd);

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
		Partition& m_partition;

		BAN::RefPtr<Inode> m_root_inode;
		BAN::Vector<uint32_t> m_superblock_backups;

		Ext2::Superblock m_superblock;

		friend class Ext2Inode;
	};

}