#include <kernel/DeviceManager.h>
#include <kernel/LockGuard.h>
#include <kernel/PCI.h>
#include <kernel/Process.h>
#include <kernel/Storage/ATAController.h>

namespace Kernel
{

	static DeviceManager* s_instance = nullptr;

	void DeviceManager::initialize()
	{
		ASSERT(s_instance == nullptr);

		s_instance = new DeviceManager;
		ASSERT(s_instance != nullptr);
		
		for (const auto& pci_device : PCI::get().devices())
		{
			switch (pci_device.class_code())
			{
				case 0x01:
				{
					StorageController* controller = nullptr;
					switch (pci_device.subclass())
					{
						case 0x01:
							if (auto res = ATAController::create(pci_device); res.is_error())
								dprintln("{}", res.error());
							else
								controller = res.value();
							break;
						default:
							dprintln("unsupported storage device (pci subclass {2H})", pci_device.subclass());
							break;
					}

					if (controller)
					{
						s_instance->add_device(controller);
						for (auto* device : controller->devices())
						{
							s_instance->add_device(device);
							for (auto* partition : device->partitions())
								s_instance->add_device(partition);
						}
					}
					break;
				}
				default:
					break;
			}
		}

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
		if (name == "."sv || name == ".."sv)
			return BAN::RefPtr<Inode>(this);
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
		TRY(result.reserve(m_devices.size() + 2));
		MUST(result.emplace_back("."sv));
		MUST(result.emplace_back(".."sv));
		for (Device* device : m_devices)
			MUST(result.emplace_back(device->name()));
		return result;
	}


}