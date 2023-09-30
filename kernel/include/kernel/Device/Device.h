#pragma once

#include <kernel/FS/RamFS/Inode.h>

namespace Kernel
{

	class Device : public RamInode
	{
	public:
		virtual ~Device() = default;
		virtual void update() {}

		virtual bool is_device() const override { return true; }
		virtual bool is_partition() const { return false; }
		virtual bool is_storage_device() const { return false; }

		virtual dev_t rdev() const override = 0;

	protected:
		Device(mode_t, uid_t, gid_t);
	};

	class BlockDevice : public Device
	{
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