#pragma once

#include <kernel/FS/TmpFS/Inode.h>
#include <kernel/Memory/MemoryRegion.h>

namespace Kernel
{

	class Device : public TmpInode
	{
	public:
		virtual ~Device() = default;
		virtual void update() {}

		virtual bool is_device() const override { return true; }
		virtual bool is_partition() const { return false; }
		virtual bool is_storage_device() const { return false; }

		virtual BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> mmap_region(PageTable&, off_t offset, size_t len, AddressRange, MemoryRegion::Type, PageTable::flags_t) { return BAN::Error::from_errno(EINVAL); }

		virtual dev_t rdev() const override = 0;

		virtual BAN::StringView name() const = 0;

	protected:
		Device(mode_t, uid_t, gid_t);
	};

	class BlockDevice : public Device
	{
	public:
		virtual BAN::ErrorOr<void> read_blocks(uint64_t first_block, size_t block_count, BAN::ByteSpan) = 0;
		virtual BAN::ErrorOr<void> write_blocks(uint64_t first_block, size_t block_count, BAN::ConstByteSpan) = 0;

		virtual blksize_t blksize() const = 0;

	protected:
		BlockDevice(mode_t mode, uid_t uid, gid_t gid)
			: Device(mode, uid, gid)
		{
			m_inode_info.mode |= Inode::Mode::IFBLK;
		}
	};

	class CharacterDevice : public Device
	{
	protected:
		CharacterDevice(mode_t mode, uid_t uid, gid_t gid)
			: Device(mode, uid, gid)
		{
			m_inode_info.mode |= Inode::Mode::IFCHR;
		}
	};

}