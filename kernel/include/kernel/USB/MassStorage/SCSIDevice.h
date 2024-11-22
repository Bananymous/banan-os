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
		USBSCSIDevice(USBMassStorageDriver& driver, uint8_t lun, BAN::UniqPtr<DMARegion>&&, uint64_t block_count, uint32_t block_size);
		~USBSCSIDevice();

		template<bool IN, typename SPAN = BAN::either_or_t<IN, BAN::ByteSpan, BAN::ConstByteSpan>>
		BAN::ErrorOr<size_t> send_scsi_command(BAN::ConstByteSpan command, SPAN data);

		template<bool IN, typename SPAN = BAN::either_or_t<IN, BAN::ByteSpan, BAN::ConstByteSpan>>
		static BAN::ErrorOr<size_t> send_scsi_command_impl(USBMassStorageDriver&, DMARegion& dma_region, uint8_t lun, BAN::ConstByteSpan command, SPAN data);

		BAN::ErrorOr<void> read_sectors_impl(uint64_t first_lba, uint64_t sector_count, BAN::ByteSpan buffer) override;
		BAN::ErrorOr<void> write_sectors_impl(uint64_t lba, uint64_t sector_count, BAN::ConstByteSpan buffer) override;

	private:
		USBMassStorageDriver& m_driver;
		BAN::UniqPtr<DMARegion> m_dma_region;

		const uint8_t m_lun;

		const uint64_t m_block_count;
		const uint32_t m_block_size;

		const dev_t m_rdev;
		const char m_name[4];

		friend class BAN::RefPtr<USBSCSIDevice>;
	};

}
