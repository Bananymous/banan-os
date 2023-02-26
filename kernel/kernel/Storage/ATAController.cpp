#include <kernel/IO.h>
#include <kernel/LockGuard.h>
#include <kernel/Storage/ATAController.h>

#define ATA_PRIMARY  	0
#define ATA_SECONDARY	1

#define ATA_PORT_DATA				0x00
#define ATA_PORT_ERROR				0x00
#define ATA_PORT_SECTOR_COUNT		0x02
#define ATA_PORT_LBA0				0x03
#define ATA_PORT_LBA1				0x04
#define ATA_PORT_LBA2				0x05
#define ATA_PORT_DRIVE_SELECT		0x06
#define ATA_PORT_COMMAND			0x07
#define ATA_PORT_STATUS				0x07

#define ATA_PORT_CONTROL			0x10
#define ATA_PORT_ALT_STATUS			0x10

#define ATA_ERROR_AMNF				0x01
#define ATA_ERROR_TKZNF				0x02
#define ATA_ERROR_ABRT				0x04
#define ATA_ERROR_MCR				0x08
#define ATA_ERROR_IDNF				0x10
#define ATA_ERROR_MC				0x20
#define ATA_ERROR_UNC				0x40
#define ATA_ERROR_BBK				0x80

#define ATA_STATUS_ERR				0x01
#define ATA_STATUS_DF				0x02
#define ATA_STATUS_DRQ				0x08
#define ATA_STATUS_BSY				0x80

#define ATA_COMMAND_READ_SECTORS	0x20
#define ATA_COMMAND_IDENTIFY		0xEC
#define ATA_COMMAND_IDENTIFY_PACKET	0xA1

#define ATA_IDENTIFY_SIGNATURE			0
#define ATA_IDENTIFY_MODEL				27
#define ATA_IDENTIFY_CAPABILITIES		49
#define ATA_IDENTIFY_LBA_COUNT			60
#define ATA_IDENTIFY_COMMAND_SET		82
#define ATA_IDENTIFY_LBA_COUNT_EXT		100
#define ATA_IDENTIFY_SECTOR_INFO		106
#define ATA_IDENTIFY_SECTOR_WORDS		117

#define ATA_COMMANDSET_LBA48_SUPPORTED	(1 << 26)

namespace Kernel
{

	BAN::ErrorOr<ATAController*> ATAController::create(const PCIDevice& device)
	{
		ATAController* controller = new ATAController(device);
		if (controller == nullptr)
			return BAN::Error::from_string("Could not allocate memory for ATAController");
		TRY(controller->initialize());
		return controller;
	}

	BAN::ErrorOr<void> ATAController::initialize()
	{
		m_buses[ATA_PRIMARY].base = 0x1F0;
		m_buses[ATA_PRIMARY].ctrl = 0x3F6;
		m_buses[ATA_PRIMARY].write(ATA_PORT_CONTROL, 2);

		m_buses[ATA_SECONDARY].base = 0x170;
		m_buses[ATA_SECONDARY].ctrl = 0x376;
		m_buses[ATA_SECONDARY].write(ATA_PORT_CONTROL, 2);

		uint8_t prog_if = m_pci_device.read_byte(0x09);
		if (prog_if & 0x01)
		{
			m_buses[ATA_PRIMARY].base = m_pci_device.read_dword(0x10) & 0xFFFFFFFC;
			m_buses[ATA_PRIMARY].ctrl = m_pci_device.read_dword(0x14) & 0xFFFFFFFC;
		}
		if (prog_if & 0x04)
		{
			m_buses[ATA_SECONDARY].base = m_pci_device.read_dword(0x18) & 0xFFFFFFFC;
			m_buses[ATA_SECONDARY].ctrl = m_pci_device.read_dword(0x1C) & 0xFFFFFFFC;
		}

		for (uint8_t bus_index = 0; bus_index < 2; bus_index++)
		{
			for (uint8_t device_index = 0; device_index < 2; device_index++)
			{
				ATABus& bus = m_buses[bus_index];
				ATADevice& device = bus.devices[device_index];

				device.type = ATADevice::Type::Unknown;
				device.slave_bit = device_index << 4;
				device.bus = &bus;

				bus.write(ATA_PORT_DRIVE_SELECT, 0xA0 | device.slave_bit);
				PIT::sleep(1);

				bus.write(ATA_PORT_COMMAND, ATA_COMMAND_IDENTIFY);
				PIT::sleep(1);

				if (bus.read(ATA_PORT_STATUS) == 0)
					continue;
				
				uint8_t status = 0;
				while (true)
				{
					status = bus.read(ATA_PORT_STATUS);
					if (status & ATA_STATUS_ERR)
						break;
					if (!(status & ATA_STATUS_BSY) && (status && ATA_STATUS_DRQ))
						break;
				}

				auto type = ATADevice::Type::ATA;

				// Not a ATA device, maybe ATAPI
				if (status & ATA_STATUS_ERR)
				{
					uint8_t lba1 = bus.read(ATA_PORT_LBA1);
					uint8_t lba2 = bus.read(ATA_PORT_LBA2);

					if (lba1 == 0x14 && lba2 == 0xEB)
						type = ATADevice::Type::ATAPI;
					else if (lba1 == 0x69 && lba2 == 0x96)
						type = ATADevice::Type::ATAPI;
					else
						continue;

					bus.write(ATA_PORT_COMMAND, ATA_COMMAND_IDENTIFY_PACKET);
					PIT::sleep(1);
				}

				uint16_t buffer[256] {};
				bus.read_buffer(ATA_PORT_DATA, buffer, 256);

				device.type			= type;
				device.signature	= *(uint16_t*)(buffer + ATA_IDENTIFY_SIGNATURE);
				device.capabilities	= *(uint16_t*)(buffer + ATA_IDENTIFY_CAPABILITIES);
				device.command_set	= *(uint32_t*)(buffer + ATA_IDENTIFY_COMMAND_SET);

				device.sector_words = 256;
				if ((buffer[ATA_IDENTIFY_SECTOR_INFO] & (1 << 15)) == 0 &&
					(buffer[ATA_IDENTIFY_SECTOR_INFO] & (1 << 14)) != 0 &&
					(buffer[ATA_IDENTIFY_SECTOR_INFO] & (1 << 12)) != 0)
				{
					device.sector_words	= *(uint32_t*)(buffer + ATA_IDENTIFY_SECTOR_WORDS);
				}

				device.lba_count = 0;
				if (device.command_set & ATA_COMMANDSET_LBA48_SUPPORTED)
					device.lba_count = *(uint64_t*)(buffer + ATA_IDENTIFY_LBA_COUNT_EXT);
				if (device.lba_count < (1 << 28))
					device.lba_count = *(uint32_t*)(buffer + ATA_IDENTIFY_LBA_COUNT);

				for (int i = 0; i < 20; i++)
				{
					uint16_t word = buffer[ATA_IDENTIFY_MODEL + i];
					device.model[2 * i + 0] = word >> 8;
					device.model[2 * i + 1] = word & 0xFF;
				}
				device.model[40] = 0;

				TRY(m_devices.push_back(&device));
			}
		}

		for (uint32_t i = 0; i < 2; i++)
		{
			for (uint32_t j = 0; j < 2; j++)
			{
				ATADevice& device = m_buses[i].devices[j];
				if (device.type == ATADevice::Type::Unknown)
					continue;
				constexpr uint32_t words_per_mib = 1024 * 1024 / 2;
				const char* device_type =
					device.type == ATADevice::Type::ATA ? "ATA" :
					device.type == ATADevice::Type::ATAPI ? "ATAPI" :
					"Unknown";
				dprintln("Found {} Drive ({} MiB) model {}", device_type, device.lba_count * device.sector_words / words_per_mib, device.model);
			}
		}

		return {};
	}

	BAN::ErrorOr<void> ATADevice::read_sectors(uint64_t lba, uint8_t sector_count, uint8_t* buffer)
	{
		if (lba + sector_count > lba_count)
			return BAN::Error::from_string("Attempted to read outside of the device boundaries");

		LockGuard _(m_lock);

		if (lba < (1 << 28))
		{
			// LBA28
			bus->write(ATA_PORT_DRIVE_SELECT, 0xE0 | slave_bit | ((lba >> 24) & 0x0F));
			bus->write(ATA_PORT_SECTOR_COUNT, sector_count);
			bus->write(ATA_PORT_LBA0, (uint8_t)(lba >>  0));
			bus->write(ATA_PORT_LBA1, (uint8_t)(lba >>  8));
			bus->write(ATA_PORT_LBA2, (uint8_t)(lba >> 16));
			bus->write(ATA_PORT_COMMAND, ATA_COMMAND_READ_SECTORS);

			for (uint32_t sector = 0; sector < sector_count; sector++)
			{
				TRY(bus->wait(true));
				bus->read_buffer(ATA_PORT_DATA, (uint16_t*)buffer + sector * sector_words, sector_words);
			}
		}
		else
		{
			// LBA48
			ASSERT(false);
		}

		return {};
	}

	uint8_t ATABus::read(uint16_t port)
	{
		if (port <= 0x07)
			return IO::inb(base + port);
		if (0x10 <= port && port <= 0x11)
			return IO::inb(ctrl + port - 0x10);
		ASSERT_NOT_REACHED();
	}

	void ATABus::read_buffer(uint16_t port, uint16_t* buffer, size_t words)
	{
		if (port <= 0x07)
			return IO::insw(base + port - 0x00, buffer, words);
		if (0x10 <= port && port <= 0x11)
			return IO::insw(ctrl + port - 0x10, buffer, words);
		ASSERT_NOT_REACHED();
	}

	void ATABus::write(uint16_t port, uint8_t data)
	{
		if (port <= 0x07)
			return IO::outb(base + port, data);
		if (0x10 <= port && port <= 0x11)
			return IO::outb(ctrl + port - 0x10, data);
		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<void> ATABus::wait(bool wait_drq)
	{
		for (uint32_t i = 0; i < 4; i++)
			read(ATA_PORT_ALT_STATUS);

		uint8_t status = ATA_STATUS_BSY;
		while (status & ATA_STATUS_BSY)
			status = read(ATA_PORT_STATUS);

		while (wait_drq && !(status & ATA_STATUS_DRQ))
		{
			if (status & ATA_STATUS_ERR)
				return error();
			if (status & ATA_STATUS_DF)
				return BAN::Error::from_string("Device fault");
			status = read(ATA_PORT_STATUS);
		}

		return {};
	}

	BAN::Error ATABus::error()
	{
		uint8_t err = read(ATA_PORT_ERROR);
		if (err & ATA_ERROR_AMNF)
			return BAN::Error::from_string("Address mark not found.");
		if (err & ATA_ERROR_TKZNF)
			return BAN::Error::from_string("Track zero not found.");
		if (err & ATA_ERROR_ABRT)
			return BAN::Error::from_string("Aborted command.");
		if (err & ATA_ERROR_MCR)
			return BAN::Error::from_string("Media change request.");
		if (err & ATA_ERROR_IDNF)
			return BAN::Error::from_string("ID not found.");
		if (err & ATA_ERROR_MC)
			return BAN::Error::from_string("Media changed.");
		if (err & ATA_ERROR_UNC)
			return BAN::Error::from_string("Uncorrectable data error.");
		if (err & ATA_ERROR_BBK)
			return BAN::Error::from_string("Bad Block detected.");
		ASSERT_NOT_REACHED();
	}

}