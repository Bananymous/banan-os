#pragma once

#include <BAN/Array.h>
#include <BAN/ByteSpan.h>
#include <kernel/Memory/Types.h>
#include <kernel/SpinLock.h>

namespace Kernel
{

	class StorageDevice;

	class DiskCache
	{
	public:
		DiskCache(size_t sector_size, StorageDevice&);
		~DiskCache();

		bool read_from_cache(uint64_t sector, BAN::ByteSpan);
		BAN::ErrorOr<void> write_to_cache(uint64_t sector, BAN::ConstByteSpan, bool dirty);

		BAN::ErrorOr<void> sync();
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
		};

	private:
		const size_t m_sector_size;
		StorageDevice& m_device;
		BAN::Vector<PageCache> m_cache;
		BAN::Array<uint8_t, PAGE_SIZE> m_sync_cache;
	};

}