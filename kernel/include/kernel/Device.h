#pragma once

#include <kernel/DeviceManager.h>

namespace Kernel
{

	class Device : public Inode
	{
	public:
		Device();
		virtual ~Device() {}
		virtual void update() {}

		virtual bool is_device() const override { return true; }

		virtual bool is_partition() const { return false; }

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
		virtual dev_t rdev() const;
		virtual BAN::StringView name() const;
		*/

	private:
		const timespec m_create_time;
		const ino_t m_ino_t;
	};

	class BlockDevice : public Device
	{
	public:
	};

	class CharacterDevice : public Device
	{
	public:
	};

}