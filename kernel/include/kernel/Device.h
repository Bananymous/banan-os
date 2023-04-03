#pragma once

#include <kernel/DeviceManager.h>

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

		virtual ino_t ino() const override { return m_ino_t; }
		virtual nlink_t nlink() const override { return 1; }
		virtual off_t size() const override { return 0; }
		virtual timespec atime() const override { return m_create_time; }
		virtual timespec mtime() const override { return m_create_time; }
		virtual timespec ctime() const override { return m_create_time; }
		virtual blksize_t blksize() const override { return DeviceManager::get().blksize(); }
		virtual blkcnt_t blocks() const override { return DeviceManager::get().blocks(); }
		virtual dev_t dev() const override { return DeviceManager::get().dev(); }

		/*
		a device has to overload
		virtual Mode mode() const;
		virtual uid_t uid() const;
		virtual gid_t gid() const;
		virutal dev_t rdev() const;
		virtual BAN::StringView name() const;
		*/

	private:
		const timespec m_create_time;
		const ino_t m_ino_t;
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