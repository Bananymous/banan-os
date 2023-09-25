#pragma once

#include <kernel/Storage/StorageDevice.h>
#include <kernel/FS/FileSystem.h>
#include <kernel/FS/Ext2/Inode.h>

namespace Kernel
{

	class Ext2FS final : public FileSystem
	{
	public:
		class BlockBufferWrapper
		{
			BAN_NON_COPYABLE(BlockBufferWrapper);
			BAN_NON_MOVABLE(BlockBufferWrapper);

		public:
			BlockBufferWrapper(BAN::Span<uint8_t> buffer, bool& used)
				: m_buffer(buffer)
				, m_used(used)
			{
				ASSERT(m_used);
			}
			~BlockBufferWrapper()
			{
				m_used = false;
			}
		
			size_t size() const { return m_buffer.size(); }

			uint8_t* data() { return m_buffer.data(); }
			const uint8_t* data() const { return m_buffer.data(); }

			uint8_t& operator[](size_t index) { return m_buffer[index]; }
			uint8_t operator[](size_t index) const { return m_buffer[index]; }

		private:
			BAN::Span<uint8_t> m_buffer;
			bool& m_used;
		};

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

		void read_block(uint32_t, BlockBufferWrapper&);
		void write_block(uint32_t, const BlockBufferWrapper&);
		void sync_superblock();

		BlockBufferWrapper get_block_buffer();

		BAN::ErrorOr<uint32_t> reserve_free_block(uint32_t primary_bgd);

		const Ext2::Superblock& superblock() const { return m_superblock; }

		struct BlockLocation
		{
			uint32_t block;
			uint32_t offset;
		};
		BlockLocation locate_inode(uint32_t);
		BlockLocation locate_block_group_descriptior(uint32_t);

		uint32_t block_size() const { return 1024 << superblock().log_block_size; }

		class BlockBufferManager
		{
		public:
			BlockBufferManager() = default;
			BlockBufferWrapper get_buffer();

			BAN::ErrorOr<void> initialize(size_t block_size);

		private:
			struct BlockBuffer
			{
				BAN::Vector<uint8_t> buffer;
				bool used { false };
			};

		private:
			BAN::Array<BlockBuffer, 10> m_buffers;
		};

	private:
		RecursiveSpinLock m_lock;

		Partition& m_partition;

		BAN::RefPtr<Inode> m_root_inode;
		BAN::Vector<uint32_t> m_superblock_backups;

		BlockBufferManager m_buffer_manager;

		Ext2::Superblock m_superblock;

		friend class Ext2Inode;
	};

}