#pragma once

#include <BAN/Vector.h>

namespace Kernel
{

	class DiskDevice
	{
	public:
		struct Partition
		{
			uint8_t type_guid[16];
			uint8_t guid[16];
			uint64_t start_lba;
			uint64_t end_lba;
			uint64_t attributes;
			char name[72];
		};

	public:
		virtual ~DiskDevice() {}

		virtual bool initialize() = 0;
		bool initialize_partitions();

		virtual bool read(uint32_t lba, uint32_t sector_count, uint8_t* buffer) = 0;

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

	private:
		BAN::Vector<DiskDevice*> m_devices;
	};
	
}