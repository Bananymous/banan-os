#include <BAN/ScopeGuard.h>
#include <kernel/LockGuard.h>
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
		}
		if (prog_if & 0x04)
		{
			buses[1].base = pci_device.read_dword(0x18) & 0xFFFFFFFC;
			buses[1].ctrl = pci_device.read_dword(0x1C) & 0xFFFFFFFC;
		}

		for (uint8_t drive = 0; drive < 4; drive++)
		{
			auto device = ATADevice::create(this, buses[drive / 2].base, buses[drive / 2].ctrl, drive);
			if (device.is_error())
			{
				if (strlen(device.error().get_message()) > 0)
					dprintln("{}", device.error());
				continue;
			}
			add_device(device.value());
		}

		for (StorageDevice* device_ : devices())
		{
			ATADevice& device = *(ATADevice*)device_;
			dprintln("Initialized drive {} ({} MiB) model {}", device.name(), device.total_size() / (1024 * 1024), device.model());
		}

		return {};
	}

	BAN::ErrorOr<void> ATAController::read(ATADevice* device, uint64_t lba, uint8_t sector_count, uint8_t* buffer)
	{
		if (lba + sector_count > device->m_lba_count)
			return BAN::Error::from_c_string("Attempted to read outside of the device boundaries");

		LockGuard _(m_lock);

		if (lba < (1 << 28))
		{
			// LBA28
			TRY(device->wait(false));
			device->io_write(ATA_PORT_DRIVE_SELECT, 0xE0 | device->m_slave_bit | ((lba >> 24) & 0x0F));
			device->io_write(ATA_PORT_SECTOR_COUNT, sector_count);
			device->io_write(ATA_PORT_LBA0, (uint8_t)(lba >>  0));
			device->io_write(ATA_PORT_LBA1, (uint8_t)(lba >>  8));
			device->io_write(ATA_PORT_LBA2, (uint8_t)(lba >> 16));
			device->io_write(ATA_PORT_COMMAND, ATA_COMMAND_READ_SECTORS);

			for (uint32_t sector = 0; sector < sector_count; sector++)
			{
				TRY(device->wait(true));
				device->read_buffer(ATA_PORT_DATA, (uint16_t*)buffer + sector * device->m_sector_words, device->m_sector_words);
			}
		}
		else
		{
			// LBA48
			ASSERT(false);
		}

		return {};
	}

	BAN::ErrorOr<void> ATAController::write(ATADevice* device, uint64_t lba, uint8_t sector_count, const uint8_t* buffer)
	{
		if (lba + sector_count > device->m_lba_count)
			return BAN::Error::from_c_string("Attempted to write outside of the device boundaries");

		LockGuard _(m_lock);

		if (lba < (1 << 28))
		{
			// LBA28
			TRY(device->wait(false));
			device->io_write(ATA_PORT_DRIVE_SELECT, 0xE0 | device->m_slave_bit | ((lba >> 24) & 0x0F));
			device->io_write(ATA_PORT_SECTOR_COUNT, sector_count);
			device->io_write(ATA_PORT_LBA0, (uint8_t)(lba >>  0));
			device->io_write(ATA_PORT_LBA1, (uint8_t)(lba >>  8));
			device->io_write(ATA_PORT_LBA2, (uint8_t)(lba >> 16));
			device->io_write(ATA_PORT_COMMAND, ATA_COMMAND_WRITE_SECTORS);

			for (uint32_t sector = 0; sector < sector_count; sector++)
			{
				TRY(device->wait(false));
				device->write_buffer(ATA_PORT_DATA, (uint16_t*)buffer + sector * device->m_sector_words, device->m_sector_words);
			}
		}
		else
		{
			// LBA48
			ASSERT(false);
		}

		TRY(device->wait(false));
		device->io_write(ATA_PORT_COMMAND, ATA_COMMAND_CACHE_FLUSH);

		return {};
	}

}