#include <BAN/ScopeGuard.h>
#include <kernel/Device/FramebufferDevice.h>
#include <kernel/Device/NullDevice.h>
#include <kernel/Device/ZeroDevice.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/FS/TmpFS/Inode.h>
#include <kernel/LockGuard.h>
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
		s_instance->add_device(MUST(NullDevice::create(0666, 0, 0)));
		s_instance->add_device(MUST(ZeroDevice::create(0666, 0, 0)));
	}

	DevFileSystem& DevFileSystem::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	void DevFileSystem::initialize_device_updater()
	{
		Process::create_kernel(
			[](void*)
			{
				while (true)
				{
					s_instance->m_device_lock.lock();
					s_instance->for_each_inode(
						[](BAN::RefPtr<TmpInode> inode)
						{
							if (inode->is_device())
								static_cast<Device*>(inode.ptr())->update();
							return BAN::Iteration::Continue;
						}
					);
					s_instance->m_device_lock.unlock();
					Scheduler::get().reschedule();
				}
			}, nullptr
		);

		auto* sync_process = Process::create_kernel();

		sync_process->add_thread(MUST(Thread::create_kernel(
			[](void*)
			{
				// NOTE: we lock the device lock here and unlock
				//       it only while semaphore is blocking
				s_instance->m_device_lock.lock();

				while (true)
				{
					while (!s_instance->m_should_sync)
					{
						s_instance->m_device_lock.unlock();
						s_instance->m_sync_semaphore.block();
						s_instance->m_device_lock.lock();
					}

					s_instance->for_each_inode(
						[](BAN::RefPtr<TmpInode> inode)
						{
							if (inode->is_device())
								if (((Device*)inode.ptr())->is_storage_device())
									if (auto ret = static_cast<StorageDevice*>(inode.ptr())->sync_disk_cache(); ret.is_error())
										dwarnln("disk sync: {}", ret.error());
							return BAN::Iteration::Continue;
						}
					);
					s_instance->m_should_sync = false;
					s_instance->m_sync_done.unblock();
				}
			}, nullptr, sync_process
		)));

		sync_process->add_thread(MUST(Kernel::Thread::create_kernel(
			[](void*)
			{
				while (true)
				{
					SystemTimer::get().sleep(10000);

					LockGuard _(s_instance->m_device_lock);
					s_instance->m_should_sync = true;
					s_instance->m_sync_semaphore.unblock();
				}
			}, nullptr, sync_process
		)));

		sync_process->register_to_scheduler();
	}

	void DevFileSystem::initiate_sync(bool should_block)
	{
		{
			LockGuard _(m_device_lock);
			m_should_sync = true;
			m_sync_semaphore.unblock();
		}
		if (should_block)
			m_sync_done.block();
	}

	void DevFileSystem::add_device(BAN::RefPtr<Device> device)
	{
		ASSERT(!device->name().contains('/'));
		MUST(static_cast<TmpDirectoryInode*>(root_inode().ptr())->link_inode(*device, device->name()));
	}

	void DevFileSystem::add_inode(BAN::StringView path, BAN::RefPtr<TmpInode> inode)
	{
		ASSERT(!path.contains('/'));
		MUST(static_cast<TmpDirectoryInode*>(root_inode().ptr())->link_inode(*inode, path));
	}

	void DevFileSystem::for_each_device(const BAN::Function<BAN::Iteration(Device*)>& callback)
	{
		LockGuard _(m_device_lock);
		for_each_inode(
			[&](BAN::RefPtr<Kernel::TmpInode> inode)
			{
				if (!inode->is_device())
					return BAN::Iteration::Continue;
				return callback(static_cast<Device*>(inode.ptr()));
			}
		);
	}

	dev_t DevFileSystem::get_next_dev() const
	{
		LockGuard _(m_device_lock);
		static dev_t next_dev = 1;
		return next_dev++;
	}

	int DevFileSystem::get_next_input_device() const
	{
		LockGuard _(m_device_lock);
		static dev_t next_dev = 0;
		return next_dev++;
	}

}