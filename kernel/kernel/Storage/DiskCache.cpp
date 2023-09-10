#include <kernel/CriticalScope.h>
#include <kernel/LockGuard.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/Storage/DiskCache.h>
#include <kernel/Storage/StorageDevice.h>

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

	bool DiskCache::read_from_cache(uint64_t sector, uint8_t* buffer)
	{
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
			page_table.map_page_at(cache.paddr, 0, PageTable::Flags::Present);
			memcpy(buffer, (void*)(page_cache_offset * m_sector_size), m_sector_size);
			page_table.unmap_page(0);

			return true;
		}

		return false;
	};

	BAN::ErrorOr<void> DiskCache::write_to_cache(uint64_t sector, const uint8_t* buffer, bool dirty)
	{
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
				page_table.map_page_at(cache.paddr, 0, PageTable::Flags::ReadWrite | PageTable::Flags::Present);
				memcpy((void*)(page_cache_offset * m_sector_size), buffer, m_sector_size);
				page_table.unmap_page(0);
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
			page_table.map_page_at(cache.paddr, 0, PageTable::Flags::Present);
			memcpy((void*)(page_cache_offset * m_sector_size), buffer, m_sector_size);
			page_table.unmap_page(0);
		}

		return {};
	}

	BAN::ErrorOr<void> DiskCache::sync()
	{
		BAN::Vector<uint8_t> sector_buffer;
		TRY(sector_buffer.resize(m_sector_size));

		PageTable& page_table = PageTable::current();
		LockGuard page_table_locker(page_table);
		ASSERT(page_table.is_page_free(0));

		for (auto& cache : m_cache)
		{
			for (int i = 0; cache.dirty_mask; i++)
			{
				if (!(cache.dirty_mask & (1 << i)))
					continue;

				{
					CriticalScope _;
					page_table.map_page_at(cache.paddr, 0, PageTable::Flags::Present);
					memcpy(sector_buffer.data(), (void*)(i * m_sector_size), m_sector_size);
					page_table.unmap_page(0);
				}

				TRY(m_device.write_sectors_impl(cache.first_sector + i, 1, sector_buffer.data()));
				cache.dirty_mask &= ~(1 << i);
			}
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