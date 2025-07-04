#pragma once

#include <BAN/HashMap.h>
#include <kernel/Device/Device.h>
#include <kernel/FS/FileSystem.h>
#include <kernel/FS/Ext2/Inode.h>

namespace Kernel
{

	class Ext2FS final : public FileSystem
	{
	public:
		virtual unsigned long bsize()   const override;
		virtual unsigned long frsize()  const override;
		virtual fsblkcnt_t    blocks()  const override;
		virtual fsblkcnt_t    bfree()   const override;
		virtual fsblkcnt_t    bavail()  const override;
		virtual fsfilcnt_t    files()   const override;
		virtual fsfilcnt_t    ffree()   const override;
		virtual fsfilcnt_t    favail()  const override;
		virtual unsigned long fsid()    const override;
		virtual unsigned long flag()    const override;
		virtual unsigned long namemax() const override;

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

			BAN::ByteSpan span() { return m_buffer; }
			BAN::ConstByteSpan span() const { return m_buffer.as_const(); }

			uint8_t& operator[](size_t index) { return m_buffer[index]; }
			uint8_t operator[](size_t index) const { return m_buffer[index]; }

		private:
			BAN::Span<uint8_t> m_buffer;
			bool& m_used;
		};

	public:
		static BAN::ErrorOr<bool> probe(BAN::RefPtr<BlockDevice>);
		static BAN::ErrorOr<BAN::RefPtr<Ext2FS>> create(BAN::RefPtr<BlockDevice>);

		virtual BAN::RefPtr<Inode> root_inode() override { return m_root_inode; }

	private:
		Ext2FS(BAN::RefPtr<BlockDevice> block_device)
			: m_block_device(block_device)
		{}

		BAN::ErrorOr<void> initialize_superblock();
		BAN::ErrorOr<void> initialize_root_inode();

		BAN::ErrorOr<uint32_t> create_inode(const Ext2::Inode&);
		BAN::ErrorOr<void> delete_inode(uint32_t ino);
		BAN::ErrorOr<void> resize_inode(uint32_t, size_t);

		BAN::ErrorOr<void> read_block(uint32_t, BlockBufferWrapper&);
		BAN::ErrorOr<void> write_block(uint32_t, const BlockBufferWrapper&);
		BAN::ErrorOr<void> sync_superblock();
		BAN::ErrorOr<void> sync_block(uint32_t block);

		BlockBufferWrapper get_block_buffer();

		BAN::ErrorOr<uint32_t> reserve_free_block(uint32_t primary_bgd);
		BAN::ErrorOr<void> release_block(uint32_t block);

		BAN::HashMap<ino_t, BAN::RefPtr<Ext2Inode>>& inode_cache() { return m_inode_cache; }

		const Ext2::Superblock& superblock() const { return m_superblock; }

		struct BlockLocation
		{
			uint32_t block;
			uint32_t offset;
		};
		BAN::ErrorOr<BlockLocation> locate_inode(uint32_t);
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
		Mutex m_mutex;

		BAN::RefPtr<BlockDevice> m_block_device;

		BAN::RefPtr<Inode> m_root_inode;
		BAN::Vector<uint32_t> m_superblock_backups;

		BAN::HashMap<ino_t, BAN::RefPtr<Ext2Inode>> m_inode_cache;

		BlockBufferManager m_buffer_manager;

		Ext2::Superblock m_superblock;

		friend class Ext2Inode;
		friend class BAN::RefPtr<Ext2FS>;
	};

}
