#include <kernel/Storage/Partition.h>

#include <sys/sysmacros.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<Partition>> Partition::create(BAN::RefPtr<BlockDevice> device, const BAN::GUID& type, const BAN::GUID& guid, uint64_t first_sector, uint64_t last_sector, uint64_t attr, const char* label, uint32_t index)
	{
		auto partition_ptr = new Partition(device, type, guid, first_sector, last_sector, attr, label, index);
		if (partition_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<Partition>::adopt(partition_ptr);
	}	

	Partition::Partition(BAN::RefPtr<BlockDevice> device, const BAN::GUID& type, const BAN::GUID& guid, uint64_t first_sector, uint64_t last_sector, uint64_t attr, const char* label, uint32_t index)
		: BlockDevice(0660, 0, 0)
		, m_device(device)
		, m_type(type)
		, m_guid(guid)
		, m_first_sector(first_sector)
		, m_last_sector(last_sector)
		, m_attributes(attr)
		, m_name(BAN::String::formatted("{}{}", device->name(), index))
		, m_rdev(makedev(major(device->rdev()), index))
	{
		memcpy(m_label, label, sizeof(m_label));
	}

	BAN::ErrorOr<void> Partition::read_sectors(uint64_t first_sector, size_t sector_count, BAN::ByteSpan buffer)
	{
		ASSERT(buffer.size() >= sector_count * m_device->blksize());
		const uint32_t sectors_in_partition = m_last_sector - m_first_sector + 1;
		if (first_sector + sector_count > sectors_in_partition)
			return BAN::Error::from_error_code(ErrorCode::Storage_Boundaries);
		TRY(m_device->read_sectors(m_first_sector + first_sector, sector_count, buffer));
		return {};
	}

	BAN::ErrorOr<void> Partition::write_sectors(uint64_t first_sector, size_t sector_count, BAN::ConstByteSpan buffer)
	{
		ASSERT(buffer.size() >= sector_count * m_device->blksize());
		const uint32_t sectors_in_partition = m_last_sector - m_first_sector + 1;
		if (m_first_sector + sector_count > sectors_in_partition)
			return BAN::Error::from_error_code(ErrorCode::Storage_Boundaries);
		TRY(m_device->write_sectors(m_first_sector + first_sector, sector_count, buffer));
		return {};
	}

	BAN::ErrorOr<size_t> Partition::read_impl(off_t offset, BAN::ByteSpan buffer)
	{
		ASSERT(offset >= 0);

		if (offset % m_device->blksize() || buffer.size() % m_device->blksize())
			return BAN::Error::from_errno(ENOTSUP);

		const uint32_t sectors_in_partition = m_last_sector - m_first_sector + 1;
		uint32_t first_sector = offset / m_device->blksize();
		uint32_t sector_count = buffer.size() / m_device->blksize();

		if (first_sector >= sectors_in_partition)
			return 0;
		if (first_sector + sector_count > sectors_in_partition)
			sector_count = sectors_in_partition - first_sector;

		TRY(read_sectors(first_sector, sector_count, buffer));
		return sector_count * m_device->blksize();
	}

}
