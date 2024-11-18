#include <kernel/Device/DeviceNumbers.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/IO.h>
#include <kernel/Storage/ATA/ATABus.h>
#include <kernel/Storage/ATA/ATADefinitions.h>
#include <kernel/Storage/ATA/ATADevice.h>

#include <sys/sysmacros.h>

namespace Kernel
{

	static dev_t get_ata_dev_minor()
	{
		static dev_t minor = 0;
		return minor++;
	}

	detail::ATABaseDevice::ATABaseDevice()
		: m_rdev(makedev(DeviceNumber::SCSI, get_ata_dev_minor()))
	{
		strcpy(m_name, "sda");
		m_name[2] += minor(m_rdev);
	}

	BAN::ErrorOr<void> detail::ATABaseDevice::initialize(BAN::Span<const uint16_t> identify_data)
	{
		ASSERT(identify_data.size() >= 256);

		m_signature    = identify_data[ATA_IDENTIFY_SIGNATURE];
		m_capabilities = identify_data[ATA_IDENTIFY_CAPABILITIES];

		m_command_set  = static_cast<uint32_t>(identify_data[ATA_IDENTIFY_COMMAND_SET + 0]) <<  0;
		m_command_set |= static_cast<uint32_t>(identify_data[ATA_IDENTIFY_COMMAND_SET + 1]) << 16;

		m_has_lba = !!(m_capabilities & ATA_CAPABILITIES_LBA);

		if ((identify_data[ATA_IDENTIFY_SECTOR_INFO] & (1 << 15)) == 0 &&
			(identify_data[ATA_IDENTIFY_SECTOR_INFO] & (1 << 14)) != 0 &&
			(identify_data[ATA_IDENTIFY_SECTOR_INFO] & (1 << 12)) != 0)
		{
			m_sector_words  = static_cast<uint32_t>(identify_data[ATA_IDENTIFY_SECTOR_WORDS + 0]) <<  0;
			m_sector_words |= static_cast<uint32_t>(identify_data[ATA_IDENTIFY_SECTOR_WORDS + 1]) << 16;
		}
		else
		{
			m_sector_words = 256;
		}

		m_lba_count = 0;
		if (m_command_set & ATA_COMMANDSET_LBA48_SUPPORTED)
		{
			m_lba_count  = static_cast<uint64_t>(identify_data[ATA_IDENTIFY_LBA_COUNT_EXT + 0]) <<  0;
			m_lba_count |= static_cast<uint64_t>(identify_data[ATA_IDENTIFY_LBA_COUNT_EXT + 1]) << 16;
			m_lba_count |= static_cast<uint64_t>(identify_data[ATA_IDENTIFY_LBA_COUNT_EXT + 2]) << 32;
			m_lba_count |= static_cast<uint64_t>(identify_data[ATA_IDENTIFY_LBA_COUNT_EXT + 3]) << 48;
		}
		if (m_lba_count < (1 << 28))
		{
			m_lba_count  = static_cast<uint32_t>(identify_data[ATA_IDENTIFY_LBA_COUNT + 0]) <<  0;
			m_lba_count |= static_cast<uint32_t>(identify_data[ATA_IDENTIFY_LBA_COUNT + 1]) << 16;
		}

		for (int i = 0; i < 20; i++)
		{
			uint16_t word = identify_data[ATA_IDENTIFY_MODEL + i];
			m_model[2 * i + 0] = word >> 8;
			m_model[2 * i + 1] = word & 0xFF;
		}
		m_model[40] = 0;

		size_t model_len = 40;
		while (model_len > 0 && m_model[model_len - 1] == ' ')
			model_len--;

		dprintln("Initialized disk '{}' {} MiB", BAN::StringView(m_model, model_len), total_size() / 1024 / 1024);

		add_disk_cache();

		DevFileSystem::get().add_device(this);
		if (auto res = initialize_partitions(name()); res.is_error())
			dprintln("{}", res.error());

		return {};
	}

	BAN::ErrorOr<BAN::RefPtr<ATADevice>> ATADevice::create(BAN::RefPtr<ATABus> bus, ATABus::DeviceType type, bool is_secondary, BAN::Span<const uint16_t> identify_data)
	{
		auto* device_ptr = new ATADevice(bus, type, is_secondary);
		if (device_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto device = BAN::RefPtr<ATADevice>::adopt(device_ptr);
		TRY(device->initialize(identify_data));
		return device;
	}

	ATADevice::ATADevice(BAN::RefPtr<ATABus> bus, ATABus::DeviceType type, bool is_secondary)
		: m_bus(bus)
		, m_type(type)
		, m_is_secondary(is_secondary)
	{ }

	BAN::ErrorOr<void> ATADevice::read_sectors_impl(uint64_t lba, uint64_t sector_count, BAN::ByteSpan buffer)
	{
		ASSERT(buffer.size() >= sector_count * sector_size());
		TRY(m_bus->read(*this, lba, sector_count, buffer));
		return {};
	}

	BAN::ErrorOr<void> ATADevice::write_sectors_impl(uint64_t lba, uint64_t sector_count, BAN::ConstByteSpan buffer)
	{
		ASSERT(buffer.size() >= sector_count * sector_size());
		TRY(m_bus->write(*this, lba, sector_count, buffer));
		return {};
	}

}
