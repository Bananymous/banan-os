#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/IO.h>
#include <kernel/Storage/ATABus.h>
#include <kernel/Storage/ATADefinitions.h>
#include <kernel/Storage/ATADevice.h>

#include <sys/sysmacros.h>

namespace Kernel
{

	ATADevice::ATADevice(ATABus& bus)
		: m_bus(bus)
		, m_rdev(makedev(DevFileSystem::get().get_next_dev(), 0))
	{ }

	BAN::ErrorOr<void> ATADevice::initialize(ATABus::DeviceType type, const uint16_t* identify_buffer)
	{
		m_type = type;

		m_signature = identify_buffer[ATA_IDENTIFY_SIGNATURE];
		m_capabilities = identify_buffer[ATA_IDENTIFY_CAPABILITIES];

		m_command_set = 0;
		m_command_set |= (uint32_t)(identify_buffer[ATA_IDENTIFY_COMMAND_SET + 0] <<  0);
		m_command_set |= (uint32_t)(identify_buffer[ATA_IDENTIFY_COMMAND_SET + 1] << 16);

		if (!(m_capabilities & ATA_CAPABILITIES_LBA))
			return BAN::Error::from_error_code(ErrorCode::ATA_NoLBA);
		
		if ((identify_buffer[ATA_IDENTIFY_SECTOR_INFO] & (1 << 15)) == 0 &&
			(identify_buffer[ATA_IDENTIFY_SECTOR_INFO] & (1 << 14)) != 0 &&
			(identify_buffer[ATA_IDENTIFY_SECTOR_INFO] & (1 << 12)) != 0)
		{
			m_sector_words = *(uint32_t*)(identify_buffer + ATA_IDENTIFY_SECTOR_WORDS);
		}
		else
		{
			m_sector_words = 256;
		}

		m_lba_count = 0;
		if (m_command_set & ATA_COMMANDSET_LBA48_SUPPORTED)
			m_lba_count = *(uint64_t*)(identify_buffer + ATA_IDENTIFY_LBA_COUNT_EXT);
		if (m_lba_count < (1 << 28))
			m_lba_count = *(uint32_t*)(identify_buffer + ATA_IDENTIFY_LBA_COUNT);

		for (int i = 0; i < 20; i++)
		{
			uint16_t word = identify_buffer[ATA_IDENTIFY_MODEL + i];
			m_model[2 * i + 0] = word >> 8;
			m_model[2 * i + 1] = word & 0xFF;
		}
		m_model[40] = 0;

		dprintln("ATA disk {} MB", total_size() / 1024 / 1024);

		add_disk_cache();

		return {};
	}

	BAN::ErrorOr<void> ATADevice::read_sectors_impl(uint64_t lba, uint8_t sector_count, uint8_t* buffer)
	{
		TRY(m_bus.read(*this, lba, sector_count, buffer));
		return {};
	}

	BAN::ErrorOr<void> ATADevice::write_sectors_impl(uint64_t lba, uint8_t sector_count, const uint8_t* buffer)
	{
		TRY(m_bus.write(*this, lba, sector_count, buffer));
		return {};
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