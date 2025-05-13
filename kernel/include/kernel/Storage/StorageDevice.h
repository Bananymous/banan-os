#pragma once

#include <BAN/Vector.h>
#include <kernel/Device/Device.h>
#include <kernel/Lock/Mutex.h>
#include <kernel/Storage/DiskCache.h>
#include <kernel/Storage/Partition.h>

namespace Kernel
{

	class StorageDevice : public BlockDevice
	{
	public:
		StorageDevice()
			: BlockDevice(0660, 0, 0)
		{ }
		virtual ~StorageDevice();

		BAN::ErrorOr<void> initialize_partitions(BAN::StringView name_prefix);

		virtual BAN::ErrorOr<void> read_blocks(uint64_t lba, size_t sector_count, BAN::ByteSpan buffer) override		{ return read_sectors(lba, sector_count, buffer); }
		virtual BAN::ErrorOr<void> write_blocks(uint64_t lba, size_t sector_count, BAN::ConstByteSpan buffer) override	{ return write_sectors(lba, sector_count, buffer); }
		virtual BAN::ErrorOr<void> sync_blocks(uint64_t block, size_t block_count) override;

		BAN::ErrorOr<void> read_sectors(uint64_t lba, size_t sector_count, BAN::ByteSpan);
		BAN::ErrorOr<void> write_sectors(uint64_t lba, size_t sector_count, BAN::ConstByteSpan);

		virtual blksize_t blksize() const override { return sector_size(); }
		virtual uint32_t sector_size() const = 0;
		virtual uint64_t total_size() const = 0;

		BAN::Vector<BAN::RefPtr<Partition>>& partitions() { return m_partitions; }
		const BAN::Vector<BAN::RefPtr<Partition>>& partitions() const { return m_partitions; }

		BAN::ErrorOr<void> sync_disk_cache();
		virtual bool is_storage_device() const override { return true; }

	protected:
		virtual BAN::ErrorOr<void> read_sectors_impl(uint64_t lba, uint64_t sector_count, BAN::ByteSpan) = 0;
		virtual BAN::ErrorOr<void> write_sectors_impl(uint64_t lba, uint64_t sector_count, BAN::ConstByteSpan) = 0;
		void add_disk_cache();

		virtual BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;
		virtual BAN::ErrorOr<size_t> write_impl(off_t, BAN::ConstByteSpan) override;

		virtual bool can_read_impl() const override { return true; }
		virtual bool can_write_impl() const override { return true; }
		virtual bool has_error_impl() const override { return false; }
		virtual bool has_hangup_impl() const override { return false; }

	private:
		Mutex								m_mutex;
		BAN::Optional<DiskCache>			m_disk_cache;
		BAN::Vector<BAN::RefPtr<Partition>>	m_partitions;

		friend class DiskCache;
	};

}
