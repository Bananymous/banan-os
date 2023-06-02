#pragma once

#include <BAN/Vector.h>
#include <kernel/Device.h>
#include <kernel/Storage/DiskCache.h>

namespace Kernel
{

	struct GUID
	{
		uint32_t data1;
		uint16_t data2;
		uint16_t data3;
		uint8_t data4[8];
	};

	class StorageDevice;

	class Partition final : public BlockDevice
	{
	public:
		Partition(StorageDevice&, const GUID&, const GUID&, uint64_t, uint64_t, uint64_t, const char*, uint32_t);

		const GUID& partition_type() const { return m_type; }
		const GUID& partition_guid() const { return m_guid; }
		uint64_t lba_start() const { return m_lba_start; }
		uint64_t lba_end() const { return m_lba_end; }
		uint64_t attributes() const { return m_attributes; }
		const char* label() const { return m_label; }
		const StorageDevice& device() const { return m_device; }

		BAN::ErrorOr<void> read_sectors(uint64_t lba, uint8_t sector_count, uint8_t* buffer);
		BAN::ErrorOr<void> write_sectors(uint64_t lba, uint8_t sector_count, const uint8_t* buffer);

	private:
		StorageDevice& m_device;
		const GUID m_type;
		const GUID m_guid;
		const uint64_t m_lba_start;
		const uint64_t m_lba_end;
		const uint64_t m_attributes;
		char m_label[36 * 4 + 1];

	public:
		virtual DeviceType device_type() const override { return DeviceType::Partition; }

		virtual Mode mode() const override { return { Mode::IFBLK | Mode::IRUSR | Mode::IRGRP }; }
		virtual uid_t uid() const override { return 0; }
		virtual gid_t gid() const override { return 0; }
		virtual dev_t rdev() const override;

		virtual BAN::StringView name() const override { return m_device_name; }

		virtual BAN::ErrorOr<size_t> read(size_t, void*, size_t) override;

	private:
		const uint32_t m_index;
		BAN::String m_device_name;
	};

	class StorageDevice : public BlockDevice
	{
	public:
		virtual ~StorageDevice();

		BAN::ErrorOr<void> initialize_partitions();

		BAN::ErrorOr<void> read_sectors(uint64_t lba, uint8_t sector_count, uint8_t* buffer);
		BAN::ErrorOr<void> write_sectors(uint64_t lba, uint8_t sector_count, const uint8_t* buffer);

		virtual uint32_t sector_size() const = 0;
		virtual uint64_t total_size() const = 0;

		BAN::Vector<Partition*>& partitions() { return m_partitions; }
		const BAN::Vector<Partition*>& partitions() const { return m_partitions; }
	
	protected:
		virtual BAN::ErrorOr<void> read_sectors_impl(uint64_t lba, uint8_t sector_count, uint8_t* buffer) = 0;
		virtual BAN::ErrorOr<void> write_sectors_impl(uint64_t lba, uint8_t sector_count, const uint8_t* buffer) = 0;
		void add_disk_cache();

	private:
		DiskCache*				m_disk_cache { nullptr };
		BAN::Vector<Partition*> m_partitions;

		friend class DiskCache;
	};

}