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

		size_t release_clean_pages(size_t);
		size_t release_pages(size_t);
		void release_all_pages();

	private:
		struct SectorCache
		{
			uint64_t sector { 0 };
			bool dirty { false };
		};
		struct CacheBlock
		{
			paddr_t paddr { 0 };
			BAN::Array<SectorCache, 4> sectors;

			void sync(StorageDevice&);
			void read_sector(StorageDevice&, size_t, uint8_t*);
			void write_sector(StorageDevice&, size_t, const uint8_t*);
		};

	private:
		SpinLock				m_lock;
		StorageDevice&			m_device;
		BAN::Vector<CacheBlock>	m_cache;
	};

}