#pragma once

#include <kernel/FS/Inode.h>

namespace Kernel
{

	class Device : public Inode
	{
	public:
		enum class DeviceType
		{
			BlockDevice,
			CharacterDevice,
			DeviceController,
			Partition,
		};

		Device();
		virtual ~Device() {}
		virtual DeviceType device_type() const = 0;
		virtual void update() {}

		virtual InodeType inode_type() const override { return InodeType::Device; }

		virtual timespec atime() const override { return m_create_time; }
		virtual timespec mtime() const override { return m_create_time; }
		virtual timespec ctime() const override { return m_create_time; }

	private:
		timespec m_create_time;
	};

	class BlockDevice : public Device
	{
	public:
		virtual DeviceType device_type() const override { return DeviceType::BlockDevice; }
	};

	class CharacterDevice : public Device
	{
	public:
		virtual DeviceType device_type() const override { return DeviceType::CharacterDevice; }
	};

}