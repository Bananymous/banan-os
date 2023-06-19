#pragma once

#include <BAN/Array.h>
#include <kernel/Memory/Types.h>
#include <kernel/SpinLock.h>

namespace Kernel
{

	class StorageDevice;

	class DiskCache
	{
	public:
		DiskCache(StorageDevice&);
		~DiskCache();

		BAN::ErrorOr<void> read_sector(uint64_t sector, uint8_t* buffer);
		BAN::ErrorOr<void> write_sector(uint64_t sector, const uint8_t* buffer);

		void sync();
		size_t release_clean_pages(size_t);
		size_t release_pages(size_t);
		void release_all_pages();

	private:
		struct PageCache
		{
			paddr_t paddr { 0 };
			uint64_t first_sector { 0 };
			uint8_t sector_mask { 0 };
			uint8_t dirty_mask { 0 };

			void sync(StorageDevice&);
			BAN::ErrorOr<void> read_sector(StorageDevice&, uint64_t sector, uint8_t* buffer);
			BAN::ErrorOr<void> write_sector(StorageDevice&, uint64_t sector, const uint8_t* buffer);
		};

	private:
		SpinLock				m_lock;
		StorageDevice&			m_device;
		BAN::Vector<PageCache>	m_cache;
	};

}