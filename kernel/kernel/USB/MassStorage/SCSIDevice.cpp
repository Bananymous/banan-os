#include <BAN/Endianness.h>

#include <kernel/Storage/SCSI.h>
#include <kernel/USB/MassStorage/Definitions.h>
#include <kernel/USB/MassStorage/SCSIDevice.h>

#include <sys/sysmacros.h>

namespace Kernel
{

	namespace SCSI
	{

		struct InquiryRes
		{
			uint8_t peripheral_device_type : 5;
			uint8_t peripheral_qualifier   : 3;

			uint8_t reserved0 : 7;
			uint8_t rmb       : 1;

			uint8_t version;

			uint8_t response_data_format : 4;
			uint8_t hisup                : 1;
			uint8_t normaca              : 1;
			uint8_t obsolete0            : 1;
			uint8_t obsolete1            : 1;

			uint8_t additional_length;

			uint8_t protect   : 1;
			uint8_t reserved1 : 2;
			uint8_t _3pc      : 1;
			uint8_t tgps      : 2;
			uint8_t acc       : 1;
			uint8_t sccs      : 1;

			uint8_t obsolete2 : 1;
			uint8_t obsolete3 : 1;
			uint8_t obsolete4 : 1;
			uint8_t obsolete5 : 1;
			uint8_t multip    : 1;
			uint8_t vs0       : 1;
			uint8_t encserv   : 1;
			uint8_t obsolete6 : 1;

			uint8_t vs1       : 1;
			uint8_t cmdque    : 1;
			uint8_t obsolete7 : 1;
			uint8_t obsolete8 : 1;
			uint8_t obsolete9 : 1;
			uint8_t obsolete10 : 1;
			uint8_t obsolete11 : 1;
			uint8_t obsolete12 : 1;

			uint8_t t10_vendor_identification[8];
			uint8_t product_identification[16];
			uint8_t product_revision_level[4];
		};
		static_assert(sizeof(InquiryRes) == 36);

		struct ReadCapacity10
		{
			BAN::BigEndian<uint32_t> logical_block_address {};
			BAN::BigEndian<uint32_t> block_length;
		};
		static_assert(sizeof(ReadCapacity10) == 8);

	}

	BAN::ErrorOr<BAN::RefPtr<USBSCSIDevice>> USBSCSIDevice::create(USBMassStorageDriver& driver, uint8_t lun, uint32_t max_packet_size)
	{
		auto dma_region = TRY(DMARegion::create(max_packet_size));

		dprintln("USB SCSI device");

		{
			const uint8_t scsi_inquiry_req[6] {
				0x12,
				0x00,
				0x00,
				0x00, sizeof(SCSI::InquiryRes),
				0x00
			};
			SCSI::InquiryRes inquiry_res;
			TRY(send_scsi_command_impl(driver, *dma_region, lun, BAN::ConstByteSpan::from(scsi_inquiry_req), BAN::ByteSpan::from(inquiry_res), true));

			dprintln("  vendor:   {}", BAN::StringView(reinterpret_cast<const char*>(inquiry_res.t10_vendor_identification), 8));
			dprintln("  product:  {}", BAN::StringView(reinterpret_cast<const char*>(inquiry_res.product_identification), 16));
			dprintln("  revision: {}", BAN::StringView(reinterpret_cast<const char*>(inquiry_res.product_revision_level), 4));
		}

		uint32_t block_count;
		uint32_t block_size;

		{
			const uint8_t scsi_read_capacity_req[10] {
				0x25,
				0x00,
				0x00, 0x00, 0x00, 0x00,
				0x00, 0x00,
				0x00,
				0x00
			};
			SCSI::ReadCapacity10 read_capacity_res;
			TRY(send_scsi_command_impl(driver, *dma_region, lun, BAN::ConstByteSpan::from(scsi_read_capacity_req), BAN::ByteSpan::from(read_capacity_res), true));

			block_count = read_capacity_res.logical_block_address + 1;
			block_size  = read_capacity_res.block_length;

			if (block_count == 0)
			{
				dwarnln("Too big USB storage");
				return BAN::Error::from_errno(ENOTSUP);
			}

			dprintln("  last LBA:   {}", block_count);
			dprintln("  block size: {}", block_size);
			dprintln("  total size: {} MiB", block_count * block_size / 1024 / 1024);
		}

		auto result = TRY(BAN::RefPtr<USBSCSIDevice>::create(driver, lun, BAN::move(dma_region), block_count, block_size));
		result->add_disk_cache();
		DevFileSystem::get().add_device(result);
		if (auto res = result->initialize_partitions(result->name()); res.is_error())
			dprintln("{}", res.error());
		return result;
	}

	USBSCSIDevice::USBSCSIDevice(USBMassStorageDriver& driver, uint8_t lun, BAN::UniqPtr<DMARegion>&& dma_region, uint64_t block_count, uint32_t block_size)
		: m_driver(driver)
		, m_dma_region(BAN::move(dma_region))
		, m_lun(lun)
		, m_block_count(block_count)
		, m_block_size(block_size)
		, m_rdev(scsi_get_rdev())
		, m_name { 's', 'd', (char)('a' + minor(m_rdev)), '\0' }
	{ }

	USBSCSIDevice::~USBSCSIDevice()
	{
		scsi_free_rdev(m_rdev);
	}

	BAN::ErrorOr<size_t> USBSCSIDevice::send_scsi_command(BAN::ConstByteSpan scsi_command, BAN::ByteSpan data, bool in)
	{
		return TRY(send_scsi_command_impl(m_driver, *m_dma_region, m_lun, scsi_command, data, in));
	}

	BAN::ErrorOr<size_t> USBSCSIDevice::send_scsi_command_impl(USBMassStorageDriver& driver, DMARegion& dma_region, uint8_t lun, BAN::ConstByteSpan scsi_command, BAN::ByteSpan data, bool in)
	{
		ASSERT(scsi_command.size() <= 16);

		LockGuard _(driver);

		auto& cbw = *reinterpret_cast<USBMassStorage::CBW*>(dma_region.vaddr());
		cbw = {
			.dCBWSignature          = 0x43425355,
			.dCBWTag                = 0x00000000,
			.dCBWDataTransferLength = static_cast<uint32_t>(data.size()),
			.bmCBWFlags             = static_cast<uint8_t>(in ? 0x80 : 0x00),
			.bCBWLUN                = lun,
			.bCBWCBLength           = static_cast<uint8_t>(scsi_command.size()),
			.CBWCB                  = {},
		};
		memcpy(cbw.CBWCB, scsi_command.data(), scsi_command.size());

		if (TRY(driver.send_bytes(dma_region.paddr(), sizeof(USBMassStorage::CBW))) != sizeof(USBMassStorage::CBW))
		{
			dwarnln("failed to send full CBW");
			return BAN::Error::from_errno(EFAULT);
		}

		const size_t ntransfer =
			TRY([&]() -> BAN::ErrorOr<size_t>
			{
				if (data.empty())
					return 0;
				if (in)
					return TRY(driver.recv_bytes(dma_region.paddr(), data.size()));
				memcpy(reinterpret_cast<void*>(dma_region.vaddr()), data.data(), data.size());
				return TRY(driver.send_bytes(dma_region.paddr(), data.size()));
			}());

		if (ntransfer > data.size())
		{
			dwarnln("device responded with more bytes than requested");
			return BAN::Error::from_errno(EFAULT);
		}

		if (in && !data.empty())
			memcpy(data.data(), reinterpret_cast<void*>(dma_region.vaddr()), ntransfer);

		if (TRY(driver.recv_bytes(dma_region.paddr(), sizeof(USBMassStorage::CSW))) != sizeof(USBMassStorage::CSW))
		{
			dwarnln("could not receive full CSW");
			return BAN::Error::from_errno(EFAULT);
		}

		if (auto status = reinterpret_cast<USBMassStorage::CSW*>(dma_region.vaddr())->bmCSWStatus)
		{
			dwarnln("CSW status {2H}", status);
			return BAN::Error::from_errno(EFAULT);
		}

		return ntransfer;
	}

	BAN::ErrorOr<void> USBSCSIDevice::read_sectors_impl(uint64_t first_lba, uint64_t sector_count, BAN::ByteSpan buffer)
	{
		const size_t max_blocks_per_read = m_dma_region->size() / m_block_size;
		ASSERT(max_blocks_per_read <= 0xFFFF);

		for (uint64_t i = 0; i < sector_count;)
		{
			const uint32_t lba = first_lba + i;
			const uint32_t count = BAN::Math::min<uint32_t>(max_blocks_per_read, sector_count - i);

			const uint8_t scsi_read_req[10] {
				0x28,
				0x00,
				(uint8_t)(lba >> 24), (uint8_t)(lba >> 16), (uint8_t)(lba >> 8), (uint8_t)(lba >> 0),
				0x00,
				(uint8_t)(count >> 8), (uint8_t)(count >> 0),
				0x00
			};
			TRY(send_scsi_command(BAN::ConstByteSpan::from(scsi_read_req), buffer.slice(i * m_block_size, count * m_block_size), true));

			i += count;
		}

		return {};
	}

	BAN::ErrorOr<void> USBSCSIDevice::write_sectors_impl(uint64_t first_lba, uint64_t sector_count, BAN::ConstByteSpan _buffer)
	{
		const size_t max_blocks_per_write = m_dma_region->size() / m_block_size;
		ASSERT(max_blocks_per_write <= 0xFFFF);

		auto buffer = BAN::ByteSpan(const_cast<uint8_t*>(_buffer.data()), _buffer.size());

		for (uint64_t i = 0; i < sector_count;)
		{
			const uint32_t lba = first_lba + i;
			const uint32_t count = BAN::Math::min<uint32_t>(max_blocks_per_write, sector_count - i);

			const uint8_t scsi_write_req[10] {
				0x2A,
				0x00,
				(uint8_t)(lba >> 24), (uint8_t)(lba >> 16), (uint8_t)(lba >> 8), (uint8_t)(lba >> 0),
				0x00,
				(uint8_t)(count >> 8), (uint8_t)(count >> 0),
				0x00
			};
			TRY(send_scsi_command(BAN::ConstByteSpan::from(scsi_write_req), buffer.slice(i * m_block_size, count * m_block_size), false));

			i += count;
		}

		return {};
	}

}
