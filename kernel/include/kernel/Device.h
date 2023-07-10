#pragma once

#include <kernel/FS/RamFS/Inode.h>

namespace Kernel
{

	class Device : public RamInode
	{
	public:
		Device(mode_t, uid_t, gid_t);
		virtual ~Device() = default;
		virtual void update() {}

		virtual bool is_device() const override { return true; }
		virtual bool is_partition() const { return false; }

		virtual dev_t rdev() const override = 0;
	};

	class BlockDevice : public Device
	{
	public:
		BlockDevice(mode_t mode, uid_t uid, gid_t gid)
			: Device(Mode::IFBLK | mode, uid, gid)
		{
			ASSERT(Device::mode().ifblk());
		}
	};

	class CharacterDevice : public Device
	{
	public:
		CharacterDevice(mode_t mode, uid_t uid, gid_t gid)
			: Device(Mode::IFCHR | mode, uid, gid)
		{
			ASSERT(Device::mode().ifchr());
		}
	};

}