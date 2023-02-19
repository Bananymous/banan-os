#pragma once

#include <BAN/Vector.h>

namespace Kernel
{

	struct GUID
	{
		uint32_t data1;
		uint16_t data2;
		uint16_t data3;
		uint8_t data4[8];
	};

	class DiskDevice
	{
	public:
		struct Partition
		{
			Partition(DiskDevice&, const GUID&, const GUID&, uint64_t, uint64_t, uint64_t, const char*);

			const GUID& type() const { return m_type; }
			const GUID& guid() const { return m_guid; }
			uint64_t lba_start() const { return m_lba_start; }
			uint64_t lba_end() const { return m_lba_end; }
			uint64_t attributes() const { return m_attributes; }
			const char* name() const { return m_name; }
			const DiskDevice& device() const { return m_device; }

			bool read_sectors(uint32_t lba, uint32_t sector_count, uint8_t* buffer);
			bool is_used() const { uint8_t zero[16] {}; return memcmp(&m_type, zero, 16); }

		private:
			DiskDevice& m_device;
			const GUID m_type;
			const GUID m_guid;
			const uint64_t m_lba_start;
			const uint64_t m_lba_end;
			const uint64_t m_attributes;
			char m_name[72];
		};

	public:
		virtual ~DiskDevice() {}

		virtual bool initialize() = 0;
		bool initialize_partitions();

		virtual bool read_sectors(uint32_t lba, uint32_t sector_count, uint8_t* buffer) = 0;
		virtual uint32_t sector_size() const = 0;

		BAN::Vector<Partition>& partitions() { return m_partitions; }

	private:
		BAN::Vector<Partition> m_partitions;
	};

	class DiskIO
	{
	public:
		static bool initialize();
		static DiskIO& get();

	private:
		DiskIO();
		void try_add_device(DiskDevice*);

	private:
		BAN::Vector<DiskDevice*> m_devices;
	};
	
}