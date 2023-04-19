#include <BAN/ScopeGuard.h>
#include <kernel/LockGuard.h>
#include <kernel/Storage/ATABus.h>
#include <kernel/Storage/ATAController.h>
#include <kernel/Storage/ATADefinitions.h>
#include <kernel/Storage/ATADevice.h>

namespace Kernel
{

	BAN::ErrorOr<ATAController*> ATAController::create(const PCIDevice& device)
	{
		ATAController* controller = new ATAController();
		if (controller == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		BAN::ScopeGuard guard([controller] { controller->unref(); });
		TRY(controller->initialize(device));
		guard.disable();
		return controller;
	}

	BAN::ErrorOr<void> ATAController::initialize(const PCIDevice& pci_device)
	{
		struct Bus
		{
			uint16_t base;
			uint16_t ctrl;
		};

		Bus buses[2];
		buses[0].base = 0x1F0;
		buses[0].ctrl = 0x3F6;

		buses[1].base = 0x170;
		buses[1].ctrl = 0x376;

		uint8_t prog_if = pci_device.read_byte(0x09);
		if (prog_if & 0x01)
		{
			buses[0].base = pci_device.read_dword(0x10) & 0xFFFFFFFC;
			buses[0].ctrl = pci_device.read_dword(0x14) & 0xFFFFFFFC;
			return BAN::Error::from_error_code(ErrorCode::ATA_UnsupportedDevice);
		}
		if (prog_if & 0x04)
		{
			buses[1].base = pci_device.read_dword(0x18) & 0xFFFFFFFC;
			buses[1].ctrl = pci_device.read_dword(0x1C) & 0xFFFFFFFC;
			return BAN::Error::from_error_code(ErrorCode::ATA_UnsupportedDevice);
		}

		m_buses[0] = ATABus::create(this, buses[0].base, buses[0].ctrl, 14);
		m_buses[1] = ATABus::create(this, buses[1].base, buses[1].ctrl, 15);

		return {};
	}

	uint8_t ATAController::next_device_index() const
	{
		static uint8_t index = 0;
		return index++;
	}

	BAN::Vector<StorageDevice*> ATAController::devices()
	{
		BAN::Vector<StorageDevice*> devices;
		if (m_buses[0]->m_devices[0])
			MUST(devices.push_back(m_buses[0]->m_devices[0]));
		if (m_buses[0]->m_devices[1])
			MUST(devices.push_back(m_buses[0]->m_devices[1]));
		if (m_buses[1]->m_devices[0])
			MUST(devices.push_back(m_buses[1]->m_devices[0]));
		if (m_buses[1]->m_devices[1])
			MUST(devices.push_back(m_buses[1]->m_devices[1]));
		return devices;
	}

}