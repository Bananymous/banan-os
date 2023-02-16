#include <BAN/Errors.h>
#include <kernel/ATA.h>
#include <kernel/Debug.h>
#include <kernel/IO.h>

#include <kernel/kprint.h>
#include <ctype.h>

#define ATA_IO_PORT_DATA			0
#define ATA_IO_PORT_ERROR			1
#define ATA_IO_PORT_FEATURES		1
#define ATA_IO_PORT_SECTOR_COUNT	2
#define ATA_IO_PORT_LBA_LOW			3
#define ATA_IO_PORT_LBA_MID			4
#define ATA_IO_PORT_LBA_HIGH		5
#define ATA_IO_PORT_DRIVE_SELECT	6
#define ATA_IO_PORT_STATUS			7
#define ATA_IO_PORT_COMMAND			7

#define ATA_CTL_PORT_STATUS			0
#define ATA_CTL_PORT_CONTROL		0
#define ATA_CTL_PORT_ADDRESS		1

#define ATA_COMMAND_READ_SECTORS	0x20
#define ATA_COMMAND_IDENTIFY		0xEC
#define ATA_COMMAND_FLUSH			0xE7

#define ATA_STATUS_ERR (1 << 0)
#define ATA_STATUS_DRQ (1 << 3)
#define ATA_STATUS_DF  (1 << 3)
#define ATA_STATUS_BSY (1 << 7)

#define ATA_DEBUG_PRINT 0

namespace Kernel
{

	enum class ATADeviceType
	{
		UNKNOWN,
		PATA,
		PATAPI,
		SATA,
		SATAPI,
	};

	static void soft_reset(uint16_t ctl_base)
	{
		IO::outb(ctl_base + ATA_CTL_PORT_CONTROL, 0b00000110);
		PIT::sleep(2);
		IO::outb(ctl_base + ATA_CTL_PORT_CONTROL, 0b00000010);
		PIT::sleep(2);
	}

	static ATADeviceType detect_device_type(uint16_t io_base, uint16_t ctl_base, uint8_t slave_bit)
	{
		soft_reset(ctl_base);

		IO::outb(io_base + ATA_IO_PORT_DRIVE_SELECT, 0xA0 | slave_bit);
		PIT::sleep(2);

		uint8_t lba_mid  = IO::inb(io_base + ATA_IO_PORT_LBA_MID);
		uint8_t lba_high = IO::inb(io_base + ATA_IO_PORT_LBA_HIGH);
		if (lba_mid == 0x00 && lba_high == 0x00)
			return ATADeviceType::PATA;
		if (lba_mid == 0x14 && lba_high == 0xEB)
			return ATADeviceType::PATAPI;
		if (lba_mid == 0x3C && lba_high == 0xC3)
			return ATADeviceType::SATA;
		if (lba_mid == 0x69 && lba_high == 0x96)
			return ATADeviceType::SATAPI;
		return ATADeviceType::UNKNOWN;
	}

	ATADevice* ATADevice::create(uint16_t io_base, uint16_t ctl_base, uint8_t slave_bit)
	{
		uint8_t status = IO::inb(io_base + ATA_IO_PORT_STATUS);
		if (status == 0xFF)
			return nullptr;

		ATADeviceType type = detect_device_type(io_base, ctl_base, slave_bit);
		if (type == ATADeviceType::UNKNOWN)
			return nullptr;

		ATADevice* ata_device = nullptr;
		switch (type)
		{
			case ATADeviceType::PATA:
				ata_device = new PATADevice(io_base, ctl_base, slave_bit);
				break;
			case ATADeviceType::PATAPI:
#if ATA_DEBUG_PRINT
				dwarnln("Unsupported PATAPI device");
#endif
				break;
			case ATADeviceType::SATA:
#if ATA_DEBUG_PRINT
				dwarnln("Unsupported SATA device");
#endif
				break;
			case ATADeviceType::SATAPI:
#if ATA_DEBUG_PRINT
				dwarnln("Unsupported SATAPI device");
#endif
				break;
			case ATADeviceType::UNKNOWN:
				break;
		}

		if (ata_device && !ata_device->initialize())
		{
			delete ata_device;
			ata_device = nullptr;
		}

		return ata_device;
	}

	bool PATADevice::initialize()
	{
		IO::outb(io_base() + ATA_IO_PORT_COMMAND, ATA_COMMAND_IDENTIFY);
		if (!wait_while_buzy() || !wait_for_transfer())
			return false;

		uint16_t response[256];
		for (int i = 0; i < 256; i++)
			response[i] = IO::inw(io_base() + ATA_IO_PORT_DATA);
		m_lba_48 = response[83] & (1 << 10);

		return true;
	}

	bool PATADevice::read(uint32_t lba, uint32_t sector_count, uint8_t* buffer)
	{
		return read_lba28(lba, sector_count, buffer);
	}

	bool PATADevice::read_lba28(uint32_t lba, uint8_t sector_count, uint8_t* buffer)
	{
		// 1. Send 0xE0 for the "master" or 0xF0 for the "slave", ORed with the highest 4 bits of the LBA to port 0x1F6:
		//    outb(0x1F6, 0xE0 | (slavebit << 4) | ((LBA >> 24) & 0x0F))
		IO::outb(io_base() + ATA_IO_PORT_DRIVE_SELECT, 0xE0 | slave_bit() | ((lba >> 24) & 0xF));

		// 2. Send a NULL byte to port 0x1F1, if you like (it is ignored and wastes lots of CPU time): outb(0x1F1, 0x00)

		// 3. Send the sectorcount to port 0x1F2: outb(0x1F2, (unsigned char) count)
		IO::outb(io_base() + ATA_IO_PORT_SECTOR_COUNT, sector_count);

		// 4. Send the low 8 bits of the LBA to port 0x1F3: outb(0x1F3, (unsigned char) LBA))
		IO::outb(io_base() + ATA_IO_PORT_LBA_LOW, lba & 0xFF);

		// 5. Send the next 8 bits of the LBA to port 0x1F4: outb(0x1F4, (unsigned char)(LBA >> 8))
		IO::outb(io_base() + ATA_IO_PORT_LBA_MID, (lba >> 8) & 0xFF);

		// 6. Send the next 8 bits of the LBA to port 0x1F5: outb(0x1F5, (unsigned char)(LBA >> 16))
		IO::outb(io_base() + ATA_IO_PORT_LBA_HIGH, (lba >> 16) & 0xFF);

		// 7. Send the "READ SECTORS" command (0x20) to port 0x1F7: outb(0x1F7, 0x20)
		IO::outb(io_base() + ATA_IO_PORT_COMMAND, ATA_COMMAND_READ_SECTORS);

		memset(buffer, 0, sector_count * 256 * sizeof(uint16_t));
		for (int i = 0; i < sector_count; i++)
		{
			// 8. Wait for an IRQ or poll.
			if (!wait_while_buzy() || !wait_for_transfer())
				return false;
			
			// 9. Transfer 256 16-bit values, a uint16_t at a time, into your buffer from I/O port 0x1F0.
			//    (In assembler, REP INSW works well for this.)
			for (int j = 0; j < 256; j++)
			{
				uint16_t word = IO::inw(io_base() + ATA_IO_PORT_DATA);
				buffer[i * 512 + j * 2 + 0] = word & 0xFF;
				buffer[i * 512 + j * 2 + 1] = word >>   8;
			}

			// 10. Then loop back to waiting for the next IRQ (or poll again -- see next note) for each successive sector.
		}

		return true;
	}

	bool PATADevice::wait_while_buzy()
	{
		uint64_t timeout = PIT::ms_since_boot() + 1000;
		while (IO::inb(ctl_base() + ATA_CTL_PORT_STATUS) & ATA_STATUS_BSY)
		{
			if (PIT::ms_since_boot() > timeout)
			{
#if ATA_DEBUG_PRINT
				dwarnln("timeout on device 0x{3H} slave: {}", io_base(), !!slave_bit());
#endif
				return false;
			}
		}
		return true;
	}

	bool PATADevice::wait_for_transfer()
	{
		uint8_t status;
		uint64_t timeout = PIT::ms_since_boot() + 1000;
		while (!((status = IO::inb(ctl_base() + ATA_CTL_PORT_STATUS)) & ATA_STATUS_DRQ))
		{
			if (status & ATA_STATUS_ERR)
			{
#if ATA_DEBUG_PRINT
				dwarnln("error on device 0x{3H} slave: {}", io_base(), !!slave_bit());
#endif
				return false;
			}
			if (status & ATA_STATUS_DF)
			{
#if ATA_DEBUG_PRINT
				dwarnln("drive fault on device 0x{3H} slave: {}", io_base(), !!slave_bit());
#endif
				return false;
			}
			if (PIT::ms_since_boot() > timeout)
			{
#if ATA_DEBUG_PRINT
				dwarnln("timeout on device 0x{3H} slave: {}", io_base(), !!slave_bit());
#endif
				return false;
			}
		}
		return true;
	}

	void PATADevice::flush()
	{
		IO::outb(io_base() + ATA_IO_PORT_COMMAND, ATA_COMMAND_FLUSH);
		wait_while_buzy();
	}

}
