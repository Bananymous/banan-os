#pragma once

#include <BAN/Vector.h>
#include <kernel/Device/Device.h>
#include <kernel/FS/TmpFS/FileSystem.h>
#include <kernel/Lock/Mutex.h>
#include <kernel/ThreadBlocker.h>

namespace Kernel
{

	class DevFileSystem final : public TmpFileSystem
	{
	public:
		static void initialize();
		static DevFileSystem& get();

		void initialize_device_updater();

		void add_device(BAN::RefPtr<Device>);
		void remove_device(BAN::RefPtr<Device>);

		void add_inode(BAN::StringView path, BAN::RefPtr<TmpInode>);

		void initiate_disk_cache_drop();
		void initiate_sync(bool should_block);

	private:
		DevFileSystem()
			: TmpFileSystem(-1)
		{ }

	private:
		mutable Mutex m_device_lock;

		BAN::Vector<BAN::RefPtr<Device>> m_devices;

		ThreadBlocker m_sync_done;
		ThreadBlocker m_sync_thread_blocker;
		volatile bool m_should_sync { false };

		SpinLock m_disk_cache_lock;
		ThreadBlocker m_disk_cache_thread_blocker;
		BAN::Atomic<bool> m_should_drop_disk_cache { false };
	};

}
