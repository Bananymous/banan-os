#include <BAN/ScopeGuard.h>
#include <kernel/IO.h>
#include <kernel/Storage/ATAController.h>
#include <kernel/Storage/ATADefinitions.h>
#include <kernel/Storage/ATADevice.h>

namespace Kernel
{

	BAN::ErrorOr<ATADevice*> ATADevice::create(ATAController* controller, uint16_t base, uint16_t ctrl, uint8_t index)
	{
		ATADevice* device = new ATADevice(controller, base, ctrl, index);
		if (device == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		BAN::ScopeGuard guard([device] { device->unref(); });
		TRY(device->initialize());
		guard.disable();
		return device;
	}

	BAN::ErrorOr<void> ATADevice::initialize()
	{
		io_write(ATA_PORT_CONTROL, ATA_CONTROL_nIEN);

		io_write(ATA_PORT_DRIVE_SELECT, 0xA0 | m_slave_bit);
		PIT::sleep(1);

		io_write(ATA_PORT_COMMAND, ATA_COMMAND_IDENTIFY);
		PIT::sleep(1);

		if (io_read(ATA_PORT_STATUS) == 0)
			return BAN::Error::from_c_string("");
		
		uint8_t status = 0;
		while (true)
		{
			status = io_read(ATA_PORT_STATUS);
			if (status & ATA_STATUS_ERR)
				break;
			if (!(status & ATA_STATUS_BSY) && (status && ATA_STATUS_DRQ))
				break;
		}

		if (status & ATA_STATUS_ERR)
		{
			uint8_t lba1 = io_read(ATA_PORT_LBA1);
			uint8_t lba2 = io_read(ATA_PORT_LBA2);

			if (lba1 == 0x14 && lba2 == 0xEB)
				m_type = ATADevice::DeviceType::ATAPI;
			else if (lba1 == 0x69 && lba2 == 0x96)
				m_type = ATADevice::DeviceType::ATAPI;
			else
				return BAN::Error::from_c_string("Not ATA/ATAPI device");

			io_write(ATA_PORT_COMMAND, ATA_COMMAND_IDENTIFY_PACKET);
			PIT::sleep(1);
		}
		else
		{
			m_type = ATADevice::DeviceType::ATA;
		}

		uint16_t buffer[256] {};
		read_buffer(ATA_PORT_DATA, buffer, 256);

		m_signature	= *(uint16_t*)(buffer + ATA_IDENTIFY_SIGNATURE);
		m_capabilities = *(uint16_t*)(buffer + ATA_IDENTIFY_CAPABILITIES);
		m_command_set = *(uint32_t*)(buffer + ATA_IDENTIFY_COMMAND_SET);

		if (!(m_capabilities & ATA_CAPABILITIES_LBA))
			return BAN::Error::from_c_string("Device does not support LBA addressing");

		if ((buffer[ATA_IDENTIFY_SECTOR_INFO] & (1 << 15)) == 0 &&
			(buffer[ATA_IDENTIFY_SECTOR_INFO] & (1 << 14)) != 0 &&
			(buffer[ATA_IDENTIFY_SECTOR_INFO] & (1 << 12)) != 0)
		{
			m_sector_words = *(uint32_t*)(buffer + ATA_IDENTIFY_SECTOR_WORDS);
		}
		else
		{
			m_sector_words = 256;
		}

		m_lba_count = 0;
		if (m_command_set & ATA_COMMANDSET_LBA48_SUPPORTED)
			m_lba_count = *(uint64_t*)(buffer + ATA_IDENTIFY_LBA_COUNT_EXT);
		if (m_lba_count < (1 << 28))
			m_lba_count = *(uint32_t*)(buffer + ATA_IDENTIFY_LBA_COUNT);

		for (int i = 0; i < 20; i++)
		{
			uint16_t word = buffer[ATA_IDENTIFY_MODEL + i];
			m_model[2 * i + 0] = word >> 8;
			m_model[2 * i + 1] = word & 0xFF;
		}
		m_model[40] = 0;
		
		m_device_name[0] = 'h';
		m_device_name[1] = 'd';
		m_device_name[2] = 'a' + m_index;
		m_device_name[3] = '\0';

		TRY(initialize_partitions());
		return {};
	}

	BAN::ErrorOr<void> ATADevice::read_sectors(uint64_t lba, uint8_t sector_count, uint8_t* buffer)
	{
		TRY(m_controller->read(this, lba, sector_count, buffer));
		return {};
	}

	BAN::ErrorOr<void> ATADevice::write_sectors(uint64_t lba, uint8_t sector_count, const uint8_t* buffer)
	{
		TRY(m_controller->write(this, lba, sector_count, buffer));
		return {};
	}

	uint8_t ATADevice::io_read(uint16_t port)
	{
		if (port <= 0x07)
			return IO::inb(m_base + port);
		if (0x10 <= port && port <= 0x11)
			return IO::inb(m_ctrl + port - 0x10);
		ASSERT_NOT_REACHED();
	}

	void ATADevice::read_buffer(uint16_t port, uint16_t* buffer, size_t words)
	{
		if (port <= 0x07)
			return IO::insw(m_base + port - 0x00, buffer, words);
		if (0x10 <= port && port <= 0x11)
			return IO::insw(m_ctrl + port - 0x10, buffer, words);
		ASSERT_NOT_REACHED();
	}

	void ATADevice::io_write(uint16_t port, uint8_t data)
	{
		if (port <= 0x07)
			return IO::outb(m_base + port, data);
		if (0x10 <= port && port <= 0x11)
			return IO::outb(m_ctrl + port - 0x10, data);
		ASSERT_NOT_REACHED();
	}

	void ATADevice::write_buffer(uint16_t port, const uint16_t* buffer, size_t words)
	{
		uint16_t io_port = 0;
		if (port <= 0x07)
			io_port = m_base + port;
		if (0x10 <= port && port <= 0x11)
			io_port = m_ctrl + port - 0x10;
		ASSERT(io_port);
		for (size_t i = 0; i < words; i++)
			IO::outw(io_port, buffer[i]);
	}

	BAN::ErrorOr<void> ATADevice::wait(bool wait_drq)
	{
		for (uint32_t i = 0; i < 4; i++)
			io_read(ATA_PORT_ALT_STATUS);

		uint8_t status = ATA_STATUS_BSY;
		while (status & ATA_STATUS_BSY)
			status = io_read(ATA_PORT_STATUS);

		while (wait_drq && !(status & ATA_STATUS_DRQ))
		{
			if (status & ATA_STATUS_ERR)
				return error();
			if (status & ATA_STATUS_DF)
				return BAN::Error::from_errno(EIO);
			status = io_read(ATA_PORT_STATUS);
		}

		return {};
	}

	BAN::Error ATADevice::error()
	{
		uint8_t err = io_read(ATA_PORT_ERROR);
		if (err & ATA_ERROR_AMNF)
			return BAN::Error::from_c_string("Address mark not found.");
		if (err & ATA_ERROR_TKZNF)
			return BAN::Error::from_c_string("Track zero not found.");
		if (err & ATA_ERROR_ABRT)
			return BAN::Error::from_c_string("Aborted command.");
		if (err & ATA_ERROR_MCR)
			return BAN::Error::from_c_string("Media change request.");
		if (err & ATA_ERROR_IDNF)
			return BAN::Error::from_c_string("ID not found.");
		if (err & ATA_ERROR_MC)
			return BAN::Error::from_c_string("Media changed.");
		if (err & ATA_ERROR_UNC)
			return BAN::Error::from_c_string("Uncorrectable data error.");
		if (err & ATA_ERROR_BBK)
			return BAN::Error::from_c_string("Bad Block detected.");
		ASSERT_NOT_REACHED();
	}

	dev_t ATADevice::dev() const
	{
		ASSERT(m_controller);
		return m_controller->dev();
	}

	BAN::ErrorOr<size_t> ATADevice::read(size_t offset, void* buffer, size_t bytes)
	{
		if (offset % sector_size() || bytes % sector_size())
			return BAN::Error::from_errno(EINVAL);
		if (offset == total_size())
			return 0;
		TRY(read_sectors(offset / sector_size(), bytes / sector_size(), (uint8_t*)buffer));
		return bytes;
	}

}