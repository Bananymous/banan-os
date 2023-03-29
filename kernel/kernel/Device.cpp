#include <kernel/Device.h>
#include <kernel/LockGuard.h>
#include <kernel/Process.h>

namespace Kernel
{

	static DeviceManager* s_instance = nullptr;

	void DeviceManager::initialize()
	{
		ASSERT(s_instance == nullptr);

		s_instance = new DeviceManager;
		ASSERT(s_instance != nullptr);
		
		MUST(Process::create_kernel(
			[](void*)
			{
				while (true)
				{
					DeviceManager::get().update();
					PIT::sleep(1);
				}
			}, nullptr)
		);
	}

	DeviceManager& DeviceManager::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	void DeviceManager::update()
	{
		LockGuard _(m_lock);
		for (Device* device : m_devices)
			device->update();
	}

	void DeviceManager::add_device(Device* device)
	{
		LockGuard _(m_lock);
		MUST(m_devices.push_back(device));
	}

	BAN::ErrorOr<BAN::RefPtr<Inode>> DeviceManager::read_directory_inode(BAN::StringView name)
	{
		LockGuard _(m_lock);
		for (Device* device : m_devices)
			if (device->name() == name)
				return BAN::RefPtr<Inode>(device);
		return BAN::Error::from_errno(ENOENT);
	}

	BAN::ErrorOr<BAN::Vector<BAN::String>> DeviceManager::read_directory_entries(size_t index)
	{
		BAN::Vector<BAN::String> result;
		if (index > 0)
			return result;

		LockGuard _(m_lock);
		TRY(result.reserve(m_devices.size()));
		for (Device* device : m_devices)
			TRY(result.emplace_back(device->name()));
		return result;
	}

}