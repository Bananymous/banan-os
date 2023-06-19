#include <kernel/LockGuard.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/Storage/DiskCache.h>
#include <kernel/Storage/StorageDevice.h>

namespace Kernel
{

	DiskCache::DiskCache(StorageDevice& device)
		: m_device(device)
	{ }

	DiskCache::~DiskCache()
	{
		release_all_pages();
	}

	BAN::ErrorOr<void> DiskCache::read_sector(uint64_t sector, uint8_t* buffer)
	{
		ASSERT(m_device.sector_size() <= PAGE_SIZE);

		LockGuard _(m_lock);

		uint64_t sectors_per_page = PAGE_SIZE / m_device.sector_size();
		ASSERT(sectors_per_page <= sizeof(PageCache::sector_mask) * 8);

		uint64_t page_cache_start = sector / sectors_per_page * sectors_per_page;

		// Check if we already have a cache for this page
		// FIXME: binary search
		size_t index = 0;
		for (; index < m_cache.size(); index++)
		{
			if (m_cache[index].first_sector < page_cache_start)
				continue;
			if (m_cache[index].first_sector > page_cache_start)
				break;
			TRY(m_cache[index].read_sector(m_device, sector, buffer));
			return {};
		}

		// Try to allocate new cache
		if (paddr_t paddr = Heap::get().take_free_page())
		{
			MUST(m_cache.insert(index, { .paddr = paddr, .first_sector = page_cache_start }));
			TRY(m_cache[index].read_sector(m_device, sector, buffer));
			return {};
		}

		// Could not allocate new cache, read from disk
		TRY(m_device.read_sectors_impl(sector, 1, buffer));
		return {};
	}

	BAN::ErrorOr<void> DiskCache::write_sector(uint64_t sector, const uint8_t* buffer)
	{
		ASSERT(m_device.sector_size() <= PAGE_SIZE);

		LockGuard _(m_lock);

		uint64_t sectors_per_page = PAGE_SIZE / m_device.sector_size();
		ASSERT(sectors_per_page <= sizeof(PageCache::sector_mask) * 8);

		uint64_t page_cache_start = sector / sectors_per_page * sectors_per_page;

		// Check if we already have a cache for this page
		// FIXME: binary search
		size_t index = 0;
		for (; index < m_cache.size(); index++)
		{
			if (m_cache[index].first_sector < page_cache_start)
				continue;
			if (m_cache[index].first_sector > page_cache_start)
				break;
			TRY(m_cache[index].write_sector(m_device, sector, buffer));
			return {};
		}

		// Try to allocate new cache
		if (paddr_t paddr = Heap::get().take_free_page())
		{
			MUST(m_cache.insert(index, { .paddr = paddr, .first_sector = page_cache_start }));
			TRY(m_cache[index].write_sector(m_device, sector, buffer));
			return {};
		}

		// Could not allocate new cache, write to disk
		TRY(m_device.write_sectors_impl(sector, 1, buffer));
		return {};
	}

	void DiskCache::sync()
	{
		ASSERT(m_device.sector_size() <= PAGE_SIZE);
	
		LockGuard _(m_lock);
		for (auto& cache_block : m_cache)
			cache_block.sync(m_device);
	}

	size_t DiskCache::release_clean_pages(size_t page_count)
	{
		ASSERT(m_device.sector_size() <= PAGE_SIZE);

		// NOTE: There might not actually be page_count pages after this
		//       function returns. The synchronization must be done elsewhere.
		LockGuard _(m_lock);

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
		ASSERT(m_device.sector_size() <= PAGE_SIZE);

		size_t released = release_clean_pages(page_count);
		if (released >= page_count)
			return released;

		// NOTE: There might not actually be page_count pages after this
		//       function returns. The synchronization must be done elsewhere.
		LockGuard _(m_lock);

		while (!m_cache.empty() && released < page_count)
		{
			m_cache.back().sync(m_device);
			Heap::get().release_page(m_cache.back().paddr);
			m_cache.pop_back();
			released++;
		}

		(void)m_cache.shrink_to_fit();

		return released;
	}

	void DiskCache::release_all_pages()
	{
		ASSERT(m_device.sector_size() <= PAGE_SIZE);

		LockGuard _(m_lock);

		for (auto& cache_block : m_cache)
		{
			cache_block.sync(m_device);
			Heap::get().release_page(cache_block.paddr);
		}

		m_cache.clear();
	}

	void DiskCache::PageCache::sync(StorageDevice& device)
	{
		if (this->dirty_mask == 0)
			return;
		
		ASSERT(device.sector_size() <= PAGE_SIZE);

		PageTable& page_table = PageTable::current();

		page_table.lock();
		ASSERT(page_table.is_page_free(0));

		page_table.map_page_at(this->paddr, 0, PageTable::Flags::ReadWrite | PageTable::Flags::Present);
		page_table.invalidate(0);

		for (size_t i = 0; i < PAGE_SIZE / device.sector_size(); i++)
		{
			if (!(this->dirty_mask & (1 << i)))
				continue;
			MUST(device.write_sectors_impl(this->first_sector + i, 1, (const uint8_t*)(i * device.sector_size())));
		}

		page_table.unmap_page(0);
		page_table.invalidate(0);

		page_table.unlock();

		this->dirty_mask = 0;
	}

	BAN::ErrorOr<void> DiskCache::PageCache::read_sector(StorageDevice& device, uint64_t sector, uint8_t* buffer)
	{
		ASSERT(device.sector_size() <= PAGE_SIZE);

		uint64_t sectors_per_page = PAGE_SIZE / device.sector_size();
		uint64_t sector_offset = sector - this->first_sector;

		ASSERT(sector_offset < sectors_per_page);

		PageTable& page_table = PageTable::current();

		page_table.lock();
		ASSERT(page_table.is_page_free(0));
		
		page_table.map_page_at(this->paddr, 0, PageTable::Flags::ReadWrite | PageTable::Flags::Present);
		page_table.invalidate(0);

		// Sector not yet cached
		if (!(this->sector_mask & (1 << sector_offset)))
		{
			TRY(device.read_sectors_impl(sector, 1, (uint8_t*)(sector_offset * device.sector_size())));
			this->sector_mask |= 1 << sector_offset;
		}
		
		memcpy(buffer, (const void*)(sector_offset * device.sector_size()), device.sector_size());

		page_table.unmap_page(0);
		page_table.invalidate(0);

		page_table.unlock();

		return {};
	}

	BAN::ErrorOr<void> DiskCache::PageCache::write_sector(StorageDevice& device, uint64_t sector, const uint8_t* buffer)
	{
		ASSERT(device.sector_size() <= PAGE_SIZE);

		uint64_t sectors_per_page = PAGE_SIZE / device.sector_size();
		uint64_t sector_offset = sector - this->first_sector;

		ASSERT(sector_offset < sectors_per_page);

		PageTable& page_table = PageTable::current();

		page_table.lock();
		ASSERT(page_table.is_page_free(0));
		
		page_table.map_page_at(this->paddr, 0, PageTable::Flags::ReadWrite | PageTable::Flags::Present);
		page_table.invalidate(0);
		
		memcpy((void*)(sector_offset * device.sector_size()), buffer, device.sector_size());
		this->sector_mask |= 1 << sector_offset;
		this->dirty_mask |= 1 << sector_offset;

		page_table.unmap_page(0);
		page_table.invalidate(0);

		page_table.unlock();

		return {};
	}

}