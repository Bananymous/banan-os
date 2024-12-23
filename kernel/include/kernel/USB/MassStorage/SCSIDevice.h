#pragma once

#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Storage/StorageDevice.h>
#include <kernel/USB/MassStorage/MassStorageDriver.h>

namespace Kernel
{

	class USBSCSIDevice : public StorageDevice
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<USBSCSIDevice>> create(USBMassStorageDriver& driver, uint8_t lun, uint32_t max_packet_size);

		uint32_t sector_size() const override { return m_block_size; }
		uint64_t total_size() const override { return m_block_size * m_block_count; }

		dev_t rdev() const override { return m_rdev; }
		BAN::StringView name() const override { return m_name; }

	private:
		USBSCSIDevice(USBMassStorageDriver& driver, uint8_t lun, uint32_t max_packet_size, uint64_t block_count, uint32_t block_size);
		~USBSCSIDevice();

		BAN::ErrorOr<void> read_sectors_impl(uint64_t first_lba, uint64_t sector_count, BAN::ByteSpan buffer) override;
		BAN::ErrorOr<void> write_sectors_impl(uint64_t lba, uint64_t sector_count, BAN::ConstByteSpan buffer) override;

	private:
		USBMassStorageDriver& m_driver;

		const uint32_t m_max_packet_size;
		const uint8_t m_lun;

		const uint64_t m_block_count;
		const uint32_t m_block_size;

		const dev_t m_rdev;
		const char m_name[4];

		friend class BAN::RefPtr<USBSCSIDevice>;
	};

}
