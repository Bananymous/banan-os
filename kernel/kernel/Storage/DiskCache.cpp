#include <BAN/ScopeGuard.h>
#include <kernel/BootInfo.h>
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

	size_t DiskCache::find_sector_cache_index(uint64_t sector) const
	{
		const uint64_t sectors_per_page = PAGE_SIZE / m_sector_size;
		const uint64_t page_cache_offset = sector % sectors_per_page;
		const uint64_t page_cache_start = sector - page_cache_offset;

		size_t l = 0, r = m_cache.size();

		while (l < r)
		{
			const size_t mid = (l + r) / 2;

			if (m_cache[mid].first_sector == page_cache_start)
				return mid;

			if (m_cache[mid].first_sector < page_cache_start)
				l = mid + 1;
			else
				r = mid;
		}

		return l;
	}

	bool DiskCache::read_from_cache(uint64_t sector, BAN::ByteSpan buffer)
	{
		ASSERT(buffer.size() >= m_sector_size);

		const uint64_t sectors_per_page = PAGE_SIZE / m_sector_size;
		const uint64_t page_cache_offset = sector % sectors_per_page;
		const uint64_t page_cache_start = sector - page_cache_offset;

		RWLockRDGuard _(m_rw_lock);

		const auto index = find_sector_cache_index(sector);
		if (index >= m_cache.size())
			return false;

		const auto& cache = m_cache[index];
		if (cache.first_sector != page_cache_start)
			return false;
		if (!(cache.sector_mask & (1 << page_cache_offset)))
			return false;

		PageTable::with_fast_page(cache.paddr, [&] {
			memcpy(buffer.data(), PageTable::fast_page_as_ptr(page_cache_offset * m_sector_size), m_sector_size);
		});

		return true;
	};

	BAN::ErrorOr<void> DiskCache::write_to_cache(uint64_t sector, BAN::ConstByteSpan buffer, bool dirty)
	{
		ASSERT(buffer.size() >= m_sector_size);

		const uint64_t sectors_per_page = PAGE_SIZE / m_sector_size;
		const uint64_t page_cache_offset = sector % sectors_per_page;
		const uint64_t page_cache_start = sector - page_cache_offset;

		RWLockWRGuard _(m_rw_lock);

		const auto index = find_sector_cache_index(sector);

		if (index >= m_cache.size() || m_cache[index].first_sector != page_cache_start)
		{
			paddr_t paddr = Heap::get().take_free_page();
			if (paddr == 0)
				return BAN::Error::from_errno(ENOMEM);

			PageCache cache {
				.paddr = paddr,
				.first_sector = page_cache_start,
				.sector_mask = 0,
				.dirty_mask = 0,
			};

			if (auto ret = m_cache.insert(index, cache); ret.is_error())
			{
				Heap::get().release_page(paddr);
				return ret.error();
			}
		}

		auto& cache = m_cache[index];

		PageTable::with_fast_page(cache.paddr, [&] {
			memcpy(PageTable::fast_page_as_ptr(page_cache_offset * m_sector_size), buffer.data(), m_sector_size);
		});

		cache.sector_mask |= 1 << page_cache_offset;
		if (dirty)
			cache.dirty_mask |= 1 << page_cache_offset;

		return {};
	}

	BAN::ErrorOr<void> DiskCache::sync_cache_index(size_t index)
	{
		LockGuard _(m_sync_mutex);

		PageCache temp_cache;

		{
			RWLockWRGuard _(m_rw_lock);

			if (index >= m_cache.size())
				return {};
			auto& cache = m_cache[index];
			if (cache.dirty_mask == 0)
				return {};

			PageTable::with_fast_page(cache.paddr, [&] {
				memcpy(m_sync_cache.data(), PageTable::fast_page_as_ptr(), PAGE_SIZE);
			});

			temp_cache = cache;
			cache.dirty_mask = 0;
			cache.syncing = true;
		}

		// restores dirty mask if write to disk fails
		BAN::ScopeGuard dirty_guard([&] {
			RWLockWRGuard _(m_rw_lock);
			const auto new_index = find_sector_cache_index(temp_cache.first_sector);
			ASSERT(new_index < m_cache.size() && m_cache[new_index].first_sector == temp_cache.first_sector);
			m_cache[new_index].dirty_mask |= temp_cache.dirty_mask;
			m_cache[new_index].syncing = false;
		});

		uint8_t sector_start = 0;
		uint8_t sector_count = 0;

		while (sector_start + sector_count <= PAGE_SIZE / m_sector_size)
		{
			if (temp_cache.dirty_mask & (1 << (sector_start + sector_count)))
				sector_count++;
			else if (sector_count == 0)
				sector_start++;
			else
			{
				dprintln_if(DEBUG_DISK_SYNC, "syncing {}->{}", temp_cache.first_sector + sector_start, temp_cache.first_sector + sector_start + sector_count);
				auto data_slice = m_sync_cache.span().slice(sector_start * m_sector_size, sector_count * m_sector_size);
				TRY(m_device.write_sectors_impl(temp_cache.first_sector + sector_start, sector_count, data_slice));
				temp_cache.dirty_mask &= ~(((1 << sector_count) - 1) << sector_start);
				sector_start += sector_count + 1;
				sector_count = 0;
			}
		}

		if (sector_count > 0)
		{
			dprintln_if(DEBUG_DISK_SYNC, "syncing {}->{}", temp_cache.first_sector + sector_start, temp_cache.first_sector + sector_start + sector_count);
			auto data_slice = m_sync_cache.span().slice(sector_start * m_sector_size, sector_count * m_sector_size);
			TRY(m_device.write_sectors_impl(temp_cache.first_sector + sector_start, sector_count, data_slice));
			temp_cache.dirty_mask &= ~(((1 << sector_count) - 1) << sector_start);
		}

		return {};
	}

	BAN::ErrorOr<void> DiskCache::sync()
	{
		if (g_disable_disk_write)
			return {};
		for (size_t i = 0; i < m_cache.size(); i++)
			TRY(sync_cache_index(i));
		return {};
	}

	BAN::ErrorOr<void> DiskCache::sync(uint64_t sector, size_t block_count)
	{
		if (g_disable_disk_write)
			return {};

		m_rw_lock.rd_lock();
		for (size_t i = find_sector_cache_index(sector); i < m_cache.size(); i++)
		{
			auto& cache = m_cache[i];
			if (cache.first_sector >= sector + block_count)
				break;
			m_rw_lock.rd_unlock();
			TRY(sync_cache_index(i));
			m_rw_lock.rd_lock();
		}
		m_rw_lock.rd_unlock();

		return {};
	}

	size_t DiskCache::release_clean_pages(size_t page_count)
	{
		// NOTE: There might not actually be page_count pages after this
		//       function returns. The synchronization must be done elsewhere.

		RWLockWRGuard _(m_rw_lock);

		size_t released = 0;
		for (size_t i = 0; i < m_cache.size() && released < page_count;)
		{
			if (!m_cache[i].syncing && m_cache[i].dirty_mask == 0)
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
