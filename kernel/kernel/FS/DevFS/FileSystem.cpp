#include <BAN/ScopeGuard.h>
#include <kernel/Device/DebugDevice.h>
#include <kernel/Device/FramebufferDevice.h>
#include <kernel/Device/NullDevice.h>
#include <kernel/Device/RandomDevice.h>
#include <kernel/Device/ZeroDevice.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/FS/TmpFS/Inode.h>
#include <kernel/Input/InputDevice.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Storage/StorageDevice.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	static DevFileSystem* s_instance = nullptr;

	void DevFileSystem::initialize()
	{
		ASSERT(s_instance == nullptr);
		s_instance = new DevFileSystem();
		ASSERT(s_instance);

		MUST(s_instance->TmpFileSystem::initialize(0755, 0, 0));
		s_instance->add_device(MUST(DebugDevice::create(0666, 0, 0)));
		s_instance->add_device(MUST(NullDevice::create(0666, 0, 0)));
		s_instance->add_device(MUST(RandomDevice::create(0666, 0, 0)));
		s_instance->add_device(MUST(ZeroDevice::create(0666, 0, 0)));
		s_instance->add_device(MUST(KeyboardDevice::create(0440, 0, 901)));
		s_instance->add_device(MUST(MouseDevice::create(0440, 0, 901)));

		// create symlink urandom -> random
		auto urandom = MUST(TmpSymlinkInode::create_new(DevFileSystem::get(), 0777, 0, 0, "random"_sv));
		s_instance->add_inode("urandom"_sv, urandom);
	}

	DevFileSystem& DevFileSystem::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	void DevFileSystem::initialize_device_updater()
	{
		Process::create_kernel(
			[](void* _devfs)
			{
				auto* devfs = static_cast<DevFileSystem*>(_devfs);
				while (true)
				{
					{
						LockGuard _(devfs->m_device_lock);
						for (auto& device : devfs->m_devices)
							device->update();
					}
					SystemTimer::get().sleep_ms(10);
				}
			}, s_instance
		);

		auto* sync_process = Process::create_kernel();

		sync_process->add_thread(MUST(Thread::create_kernel(
			[](void* _devfs)
			{
				auto* devfs = static_cast<DevFileSystem*>(_devfs);
				while (true)
				{
					LockGuard _(devfs->m_device_lock);
					while (!devfs->m_should_sync)
						devfs->m_sync_thread_blocker.block_indefinite(&devfs->m_device_lock);

					for (auto& device : devfs->m_devices)
						if (device->is_storage_device())
							if (auto ret = static_cast<StorageDevice*>(device.ptr())->sync_disk_cache(); ret.is_error())
								dwarnln("disk sync: {}", ret.error());

					devfs->m_should_sync = false;
					devfs->m_sync_done.unblock();
				}
			}, s_instance, sync_process
		)));

		sync_process->add_thread(MUST(Kernel::Thread::create_kernel(
			[](void* _devfs)
			{
				auto* devfs = static_cast<DevFileSystem*>(_devfs);
				while (true)
				{
					SystemTimer::get().sleep_ms(10'000);
					devfs->initiate_sync(false);
				}
			}, s_instance, sync_process
		)));

		sync_process->register_to_scheduler();
	}

	void DevFileSystem::initiate_sync(bool should_block)
	{
		LockGuard _(m_device_lock);
		m_should_sync = true;
		m_sync_thread_blocker.unblock();
		while (should_block && m_should_sync)
			m_sync_done.block_indefinite(&m_device_lock);
	}

	void DevFileSystem::add_device(BAN::RefPtr<Device> device)
	{
		LockGuard _(m_device_lock);
		ASSERT(!device->name().contains('/'));
		MUST(static_cast<TmpDirectoryInode*>(root_inode().ptr())->link_inode(*device, device->name()));
		MUST(m_devices.push_back(device));

		dprintln("Added device /dev/{}", device->name());
	}

	void DevFileSystem::remove_device(BAN::RefPtr<Device> device)
	{
		LockGuard _(m_device_lock);
		ASSERT(!device->name().contains('/'));
		MUST(static_cast<TmpDirectoryInode*>(root_inode().ptr())->unlink(device->name()));
		for (size_t i = 0; i < m_devices.size(); i++)
		{
			if (m_devices[i] == device)
			{
				m_devices.remove(i);
				break;
			}
		}

		dprintln("Removed device /dev/{}", device->name());
	}

	void DevFileSystem::add_inode(BAN::StringView path, BAN::RefPtr<TmpInode> inode)
	{
		ASSERT(!inode->is_device());
		ASSERT(!path.contains('/'));
		MUST(static_cast<TmpDirectoryInode*>(root_inode().ptr())->link_inode(*inode, path));
	}

}
