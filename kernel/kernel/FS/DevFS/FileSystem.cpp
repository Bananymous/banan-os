#include <BAN/ScopeGuard.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/FS/RamFS/Inode.h>
#include <kernel/LockGuard.h>
#include <kernel/Process.h>

namespace Kernel
{

	static DevFileSystem* s_instance = nullptr;

	void DevFileSystem::initialize()
	{
		ASSERT(s_instance == nullptr);
		s_instance = new DevFileSystem(1024 * 1024);
		ASSERT(s_instance);

		auto root_inode = MUST(RamDirectoryInode::create(*s_instance, 0, Inode::Mode::IFDIR | 0755, 0, 0));
		MUST(s_instance->set_root_inode(root_inode));
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
						[](BAN::RefPtr<RamInode> inode)
						{
							if (inode->is_device())
								((Device*)inode.ptr())->update();
						}
					);
					s_instance->m_device_lock.unlock();

					PIT::sleep(1);
				}
			}, nullptr
		);
	}

	void DevFileSystem::add_device(BAN::StringView path, BAN::RefPtr<Device> device)
	{
		ASSERT(!path.contains('/'));
		MUST(reinterpret_cast<RamDirectoryInode*>(root_inode().ptr())->add_inode(path, device));
	}

	dev_t DevFileSystem::get_next_rdev()
	{
		static dev_t next_rdev = 1;
		return next_rdev++;
	}

}