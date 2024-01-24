#include <kernel/CriticalScope.h>
#include <kernel/LockGuard.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/Storage/DiskCache.h>
#include <kernel/Storage/StorageDevice.h>

#define DEBUG_SYNC 0

namespace Kernel
{

	DiskCache::DiskCache(size_t sector_size, StorageDevice& device)
		: m_sector_size(sector_size)
		, m_device(device)
	{
		ASSERT(PAGE_SIZE % m_sector_size == 0);
		ASSERT(PAGE_SIZE / m_sector_size <= sizeof(PageCache::sector_mask) * 8);
		ASSERT(PAGE_SIZE / m_sector_size <= sizeof(PageCache::dirty_mask)  * 8);
	}

	DiskCache::~DiskCache()
	{
		release_all_pages();
	}

	bool DiskCache::read_from_cache(uint64_t sector, BAN::ByteSpan buffer)
	{
		ASSERT(buffer.size() >= m_sector_size);

		uint64_t sectors_per_page = PAGE_SIZE / m_sector_size;
		uint64_t page_cache_offset = sector % sectors_per_page;
		uint64_t page_cache_start = sector - page_cache_offset;

		PageTable& page_table = PageTable::current();
		LockGuard page_table_locker(page_table);
		ASSERT(page_table.is_page_free(0));

		for (auto& cache : m_cache)
		{
			if (cache.first_sector < page_cache_start)
				continue;
			if (cache.first_sector > page_cache_start)
				break;

			if (!(cache.sector_mask & (1 << page_cache_offset)))
				continue;

			CriticalScope _;
			PageTable::map_fast_page(cache.paddr);
			memcpy(buffer.data(), PageTable::fast_page_as_ptr(page_cache_offset * m_sector_size), m_sector_size);
			PageTable::unmap_fast_page();

			return true;
		}

		return false;
	};

	BAN::ErrorOr<void> DiskCache::write_to_cache(uint64_t sector, BAN::ConstByteSpan buffer, bool dirty)
	{
		ASSERT(buffer.size() >= m_sector_size);
		uint64_t sectors_per_page = PAGE_SIZE / m_sector_size;
		uint64_t page_cache_offset = sector % sectors_per_page;
		uint64_t page_cache_start = sector - page_cache_offset;

		PageTable& page_table = PageTable::current();
		LockGuard page_table_locker(page_table);
		ASSERT(page_table.is_page_free(0));

		size_t index = 0;

		// Search the cache if the have this sector in memory
		for (; index < m_cache.size(); index++)
		{
			auto& cache = m_cache[index];

			if (cache.first_sector < page_cache_start)
				continue;
			if (cache.first_sector > page_cache_start)
				break;

			{
				CriticalScope _;
				PageTable::map_fast_page(cache.paddr);
				memcpy(PageTable::fast_page_as_ptr(page_cache_offset * m_sector_size), buffer.data(), m_sector_size);
				PageTable::unmap_fast_page();
			}

			cache.sector_mask |= 1 << page_cache_offset;
			if (dirty)
				cache.dirty_mask |= 1 << page_cache_offset;

			return {};
		}

		// Try to add new page to the cache
		paddr_t paddr = Heap::get().take_free_page();
		if (paddr == 0)
			return BAN::Error::from_errno(ENOMEM);

		PageCache cache;
		cache.paddr			= paddr;
		cache.first_sector	= page_cache_start;
		cache.sector_mask	= 1 << page_cache_offset;
		cache.dirty_mask	= dirty ? cache.sector_mask : 0;

		if (auto ret = m_cache.insert(index, cache); ret.is_error())
		{
			Heap::get().release_page(paddr);
			return ret.error();
		}

		{
			CriticalScope _;
			PageTable::map_fast_page(cache.paddr);
			memcpy(PageTable::fast_page_as_ptr(page_cache_offset * m_sector_size), buffer.data(), m_sector_size);
			PageTable::unmap_fast_page();
		}

		return {};
	}

	BAN::ErrorOr<void> DiskCache::sync()
	{
		for (auto& cache : m_cache)
		{
			if (cache.dirty_mask == 0)
				continue;

			{
				CriticalScope _;
				PageTable::map_fast_page(cache.paddr);
				memcpy(m_sync_cache.data(), PageTable::fast_page_as_ptr(), PAGE_SIZE);
				PageTable::unmap_fast_page();
			}

			uint8_t sector_start = 0;
			uint8_t sector_count = 0;

			while (sector_start + sector_count <= PAGE_SIZE / m_sector_size)
			{
				if (cache.dirty_mask & (1 << (sector_start + sector_count)))
					sector_count++;
				else if (sector_count == 0)
					sector_start++;
				else
				{
					dprintln_if(DEBUG_SYNC, "syncing {}->{}", cache.first_sector + sector_start, cache.first_sector + sector_start + sector_count);
					auto data_slice = m_sync_cache.span().slice(sector_start * m_sector_size, sector_count * m_sector_size);
					TRY(m_device.write_sectors_impl(cache.first_sector + sector_start, sector_count, data_slice));
					sector_start += sector_count + 1;
					sector_count = 0;
				}
			}

			if (sector_count > 0)
			{
				dprintln_if(DEBUG_SYNC, "syncing {}->{}", cache.first_sector + sector_start, cache.first_sector + sector_start + sector_count);
				auto data_slice = m_sync_cache.span().slice(sector_start * m_sector_size, sector_count * m_sector_size);
				TRY(m_device.write_sectors_impl(cache.first_sector + sector_start, sector_count, data_slice));
			}

			cache.dirty_mask = 0;
		}

		return {};
	}

	size_t DiskCache::release_clean_pages(size_t page_count)
	{
		// NOTE: There might not actually be page_count pages after this
		//       function returns. The synchronization must be done elsewhere.

		size_t released = 0;
		for (size_t i = 0; i < m_cache.size() && released < page_count;)
		{
			if (m_cache[i].dirty_mask == 0)
			{
				Heap::get().release_page(m_cache[i].paddr);
				m_cache.remove(i);
				released++;
				continue;
			}
			i++;
		}

		(void)m_cache.shrink_to_fit();

		return released;
	}

	size_t DiskCache::release_pages(size_t page_count)
	{
		size_t released = release_clean_pages(page_count);
		if (released >= page_count)
			return released;
		if (!sync().is_error())
			released += release_clean_pages(page_count - released);
		return released;
	}

	void DiskCache::release_all_pages()
	{
		release_pages(m_cache.size());
	}

}
