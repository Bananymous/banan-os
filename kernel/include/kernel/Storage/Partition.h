#pragma once

#include <BAN/GUID.h>
#include <kernel/Device/Device.h>

namespace Kernel
{

	class Partition final : public BlockDevice
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<Partition>> create(BAN::RefPtr<BlockDevice>, const BAN::GUID& type, const BAN::GUID& guid, uint64_t first_sector, uint64_t last_sector, uint64_t attr, const char* label, uint32_t index);

		const BAN::GUID& partition_type() const { return m_type; }
		const BAN::GUID& partition_guid() const { return m_guid; }
		const BAN::RefPtr<BlockDevice> device() const { return m_device; }

		virtual blksize_t blksize() const { return m_device->blksize(); }

		virtual BAN::ErrorOr<void> read_sectors(uint64_t first_sector, size_t sector_count, BAN::ByteSpan) override;
		virtual BAN::ErrorOr<void> write_sectors(uint64_t first_sector, size_t sector_count, BAN::ConstByteSpan) override;

		virtual BAN::StringView name() const override { return m_name; }

	private:
		Partition(BAN::RefPtr<BlockDevice>, const BAN::GUID&, const BAN::GUID&, uint64_t, uint64_t, uint64_t, const char*, uint32_t);

	private:
		BAN::RefPtr<BlockDevice> m_device;
		const BAN::GUID m_type;
		const BAN::GUID m_guid;
		const uint64_t m_first_sector;
		const uint64_t m_last_sector;
		const uint64_t m_attributes;
		char m_label[36 * 4 + 1];
		const BAN::String m_name;

	public:
		virtual bool is_partition() const override { return true; }

		virtual dev_t rdev() const override { return m_rdev; }

	protected:
		virtual BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;

	private:
		const dev_t m_rdev;
	};

}
