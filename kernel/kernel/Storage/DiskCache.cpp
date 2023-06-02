#include <kernel/LockGuard.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/PageTableScope.h>
#include <kernel/Storage/DiskCache.h>
#include <kernel/Storage/StorageDevice.h>

namespace Kernel
{

	DiskCache::DiskCache(StorageDevice& device)
		: m_device(device)
	{ }

	DiskCache::~DiskCache()
	{
		if (m_device.sector_size() == 0)
			return;
		release_all_pages();
	}

	BAN::ErrorOr<void> DiskCache::read_sector(uint64_t sector, uint8_t* buffer)
	{
		LockGuard _(m_lock);

		ASSERT(m_device.sector_size() > 0);
		ASSERT(m_device.sector_size() <= PAGE_SIZE);

		for (auto& cache_block : m_cache)
		{
			for (size_t i = 0; i < cache_block.sectors.size(); i++)
			{
				if (cache_block.sectors[i].sector != sector)
					continue;
				cache_block.read_sector(m_device, i, buffer);
				return {};
			}
		}

		// Sector was not cached so we must read it from disk
		TRY(m_device.read_sectors_impl(sector, 1, buffer));
		
		// We try to add the sector to exisiting cache block
		if (!m_cache.empty())
		{
			auto& cache_block = m_cache.back();
			for (size_t i = 0; i < m_cache.back().sectors.size(); i++)
			{
				if (cache_block.sectors[i].sector)
					continue;
				cache_block.write_sector(m_device, i, buffer);
				cache_block.sectors[i].sector = sector;
				cache_block.sectors[i].dirty = false;
				return {};
			}
		}

		// We try to allocate new cache block for this sector
		if (!m_cache.emplace_back().is_error())
		{
			if (paddr_t paddr = Heap::get().take_free_page())
			{
				auto& cache_block = m_cache.back();
				cache_block.paddr = paddr;
				cache_block.write_sector(m_device, 0, buffer);
				cache_block.sectors[0].sector = sector;
				cache_block.sectors[0].dirty = false;
				return {};
			}
		}

		// We could not cache the sector
		return {};
	}

	BAN::ErrorOr<void> DiskCache::write_sector(uint64_t sector, const uint8_t* buffer)
	{
		LockGuard _(m_lock);

		ASSERT(m_device.sector_size() > 0);
		ASSERT(m_device.sector_size() <= PAGE_SIZE);
		
		// Try to find this sector in the cache
		for (auto& cache_block : m_cache)
		{
			for (size_t i = 0; i < cache_block.sectors.size(); i++)
			{
				if (cache_block.sectors[i].sector != sector)
					continue;
				cache_block.write_sector(m_device, i, buffer);
				cache_block.sectors[i].dirty = true;
				return {};
			}
		}

		// Sector was not in the cache, we try to add it to exisiting cache block
		if (!m_cache.empty())
		{
			auto& cache_block = m_cache.back();
			for (size_t i = 0; i < m_cache.back().sectors.size(); i++)
			{
				if (cache_block.sectors[i].sector)
					continue;
				cache_block.write_sector(m_device, i, buffer);
				cache_block.sectors[i].sector = sector;
				cache_block.sectors[i].dirty = true;
				return {};
			}
		}

		// We try to allocate new cache block
		if (!m_cache.emplace_back().is_error())
		{
			if (paddr_t paddr = Heap::get().take_free_page())
			{
				auto& cache_block = m_cache.back();
				cache_block.paddr = paddr;
				cache_block.write_sector(m_device, 0, buffer);
				cache_block.sectors[0].sector = sector;
				cache_block.sectors[0].dirty = true;
				return {};
			}
		}

		// We could not allocate cache, so we must sync it to disk
		// right away
		TRY(m_device.write_sectors_impl(sector, 1, buffer));
		return {};
	}

	size_t DiskCache::release_clean_pages(size_t page_count)
	{
		LockGuard _(m_lock);

		ASSERT(m_device.sector_size() > 0);
		ASSERT(m_device.sector_size() <= PAGE_SIZE);

		size_t released = 0;
		for (size_t i = 0; i < m_cache.size() && released < page_count;)
		{
			bool dirty = false;
			for (size_t j = 0; j < sizeof(m_cache[i].sectors) / sizeof(SectorCache); j++)
				if (m_cache[i].sectors[j].dirty)
					dirty = true;
			if (dirty)
			{
				i++;
				continue;
			}

			Heap::get().release_page(m_cache[i].paddr);
			m_cache.remove(i);
			released++;
		}

		return released;
	}

	size_t DiskCache::release_pages(size_t page_count)
	{
		ASSERT(m_device.sector_size() > 0);
		ASSERT(m_device.sector_size() <= PAGE_SIZE);

		size_t released = release_clean_pages(page_count);
		if (released >= page_count)
			return page_count;

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

		return released;
	}

	void DiskCache::release_all_pages()
	{
		LockGuard _(m_lock);

		ASSERT(m_device.sector_size() > 0);
		ASSERT(m_device.sector_size() <= PAGE_SIZE);

		uint8_t* temp_buffer = (uint8_t*)kmalloc(m_device.sector_size());
		ASSERT(temp_buffer);

		while (!m_cache.empty())
		{
			auto& cache_block = m_cache.back();
			cache_block.sync(m_device);
			Heap::get().release_page(cache_block.paddr);
			m_cache.pop_back();
		}
	}


	void DiskCache::CacheBlock::sync(StorageDevice& device)
	{
		uint8_t* temp_buffer = (uint8_t*)kmalloc(device.sector_size());
		ASSERT(temp_buffer);

		for (size_t i = 0; i < sectors.size(); i++)
		{
			if (!sectors[i].dirty)
				continue;
			read_sector(device, i, temp_buffer);
			MUST(device.write_sectors_impl(sectors[i].sector, 1, temp_buffer));
			sectors[i].dirty = false;
		}

		kfree(temp_buffer);	
	}

	void DiskCache::CacheBlock::read_sector(StorageDevice& device, size_t index, uint8_t* buffer)
	{
		ASSERT(index < sectors.size());
		
		PageTableScope _(PageTable::current());
		ASSERT(PageTable::current().is_page_free(0));
		PageTable::current().map_page_at(paddr, 0, PageTable::Flags::Present);
		memcpy(buffer, (void*)(index * device.sector_size()), device.sector_size());
		PageTable::current().unmap_page(0);
		PageTable::current().invalidate(0);
	}

	void DiskCache::CacheBlock::write_sector(StorageDevice& device, size_t index, const uint8_t* buffer)
	{
		ASSERT(index < sectors.size());
		
		PageTableScope _(PageTable::current());
		ASSERT(PageTable::current().is_page_free(0));
		PageTable::current().map_page_at(paddr, 0, PageTable::Flags::ReadWrite | PageTable::Flags::Present);
		memcpy((void*)(index * device.sector_size()), buffer, device.sector_size());
		PageTable::current().unmap_page(0);
		PageTable::current().invalidate(0);
	}

}