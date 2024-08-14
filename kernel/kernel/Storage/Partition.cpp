#include <kernel/Storage/Partition.h>

#include <sys/sysmacros.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<Partition>> Partition::create(BAN::RefPtr<BlockDevice> device, const BAN::GUID& type, const BAN::GUID& guid, uint64_t first_block, uint64_t last_block, uint64_t attr, const char* label, uint32_t index, BAN::StringView name_prefix)
	{
		auto partition_ptr = new Partition(device, type, guid, first_block, last_block, attr, label, index, name_prefix);
		if (partition_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<Partition>::adopt(partition_ptr);
	}

	Partition::Partition(BAN::RefPtr<BlockDevice> device, const BAN::GUID& type, const BAN::GUID& guid, uint64_t first_block, uint64_t last_block, uint64_t attr, const char* label, uint32_t index, BAN::StringView name_prefix)
		: BlockDevice(0660, 0, 0)
		, m_device(device)
		, m_type(type)
		, m_guid(guid)
		, m_guid_string(MUST(guid.to_string()))
		, m_first_block(first_block)
		, m_last_block(last_block)
		, m_attributes(attr)
		, m_name(MUST(BAN::String::formatted("{}{}", name_prefix, index)))
		, m_rdev(makedev(major(device->rdev()), index))
	{
		memcpy(m_label, label, sizeof(m_label));
	}

	BAN::ErrorOr<void> Partition::read_blocks(uint64_t first_block, size_t block_count, BAN::ByteSpan buffer)
	{
		ASSERT(buffer.size() >= block_count * m_device->blksize());
		const uint32_t blocks_in_partition = m_last_block - m_first_block + 1;
		if (first_block + block_count > blocks_in_partition)
			return BAN::Error::from_error_code(ErrorCode::Storage_Boundaries);
		TRY(m_device->read_blocks(m_first_block + first_block, block_count, buffer));
		return {};
	}

	BAN::ErrorOr<void> Partition::write_blocks(uint64_t first_block, size_t block_count, BAN::ConstByteSpan buffer)
	{
		ASSERT(buffer.size() >= block_count * m_device->blksize());
		const uint32_t blocks_in_partition = m_last_block - m_first_block + 1;
		if (first_block + block_count > blocks_in_partition)
			return BAN::Error::from_error_code(ErrorCode::Storage_Boundaries);
		TRY(m_device->write_blocks(m_first_block + first_block, block_count, buffer));
		return {};
	}

	BAN::ErrorOr<size_t> Partition::read_impl(off_t offset, BAN::ByteSpan buffer)
	{
		ASSERT(offset >= 0);

		if (offset % m_device->blksize() || buffer.size() % m_device->blksize())
			return BAN::Error::from_errno(ENOTSUP);

		const uint32_t blocks_in_partition = m_last_block - m_first_block + 1;
		uint32_t first_block = offset / m_device->blksize();
		uint32_t block_count = buffer.size() / m_device->blksize();

		if (first_block >= blocks_in_partition)
			return 0;
		if (first_block + block_count > blocks_in_partition)
			block_count = blocks_in_partition - first_block;

		TRY(read_blocks(first_block, block_count, buffer));
		return block_count * m_device->blksize();
	}

}
