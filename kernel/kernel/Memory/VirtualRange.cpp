#include <kernel/Memory/Heap.h>
#include <kernel/Memory/VirtualRange.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> VirtualRange::create_to_vaddr(PageTable& page_table, vaddr_t vaddr, size_t size, PageTable::flags_t flags, bool preallocate_pages, bool add_guard_pages)
	{
		ASSERT(size % PAGE_SIZE == 0);
		ASSERT(vaddr % PAGE_SIZE == 0);
		ASSERT(vaddr > 0);

		if (add_guard_pages)
		{
			vaddr -= PAGE_SIZE;
			size += 2 * PAGE_SIZE;
		}

		auto result = TRY(BAN::UniqPtr<VirtualRange>::create(page_table, preallocate_pages, add_guard_pages, vaddr, size, flags));
		ASSERT(page_table.reserve_range(vaddr, size));
		TRY(result->initialize());

		return result;
	}

	BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> VirtualRange::create_to_vaddr_range(PageTable& page_table, vaddr_t vaddr_start, vaddr_t vaddr_end, size_t size, PageTable::flags_t flags, bool preallocate_pages, bool add_guard_pages)
	{
		if (add_guard_pages)
			size += 2 * PAGE_SIZE;

		ASSERT(size % PAGE_SIZE == 0);
		ASSERT(vaddr_start > 0);
		ASSERT(vaddr_start + size <= vaddr_end);

		// Align vaddr range to page boundaries
		if (size_t rem = vaddr_start % PAGE_SIZE)
			vaddr_start += PAGE_SIZE - rem;
		if (size_t rem = vaddr_end % PAGE_SIZE)
			vaddr_end -= rem;
		ASSERT(vaddr_start < vaddr_end);
		ASSERT(vaddr_end - vaddr_start + 1 >= size / PAGE_SIZE);

		const vaddr_t vaddr = page_table.reserve_free_contiguous_pages(size / PAGE_SIZE, vaddr_start, vaddr_end);
		if (vaddr == 0)
			return BAN::Error::from_errno(ENOMEM);
		ASSERT(vaddr >= vaddr_start);
		ASSERT(vaddr + size <= vaddr_end);

		auto result_or_error = BAN::UniqPtr<VirtualRange>::create(page_table, preallocate_pages, add_guard_pages, vaddr, size, flags);
		if (result_or_error.is_error())
		{
			page_table.unmap_range(vaddr, size);
			return result_or_error.release_error();
		}

		auto result = result_or_error.release_value();
		TRY(result->initialize());

		return result;
	}

	VirtualRange::VirtualRange(PageTable& page_table, bool preallocated, bool has_guard_pages, vaddr_t vaddr, size_t size, PageTable::flags_t flags)
		: m_page_table(page_table)
		, m_preallocated(preallocated)
		, m_has_guard_pages(has_guard_pages)
		, m_vaddr(vaddr)
		, m_size(size)
		, m_flags(flags)
	{ }

	VirtualRange::~VirtualRange()
	{
		ASSERT(m_vaddr);
		m_page_table.unmap_range(m_vaddr, m_size);

		for (paddr_t paddr : m_paddrs)
			if (paddr != 0)
				Heap::get().release_page(paddr);
	}

	BAN::ErrorOr<void> VirtualRange::initialize()
	{
		TRY(m_paddrs.resize(size() / PAGE_SIZE, 0));

		if (!m_preallocated)
			return {};

		const size_t page_count = size() / PAGE_SIZE;
		for (size_t i = 0; i < page_count; i++)
		{
			m_paddrs[i] = Heap::get().take_free_page();
			if (m_paddrs[i] == 0)
				return BAN::Error::from_errno(ENOMEM);
			m_page_table.map_page_at(m_paddrs[i], vaddr() + i * PAGE_SIZE, m_flags);
		}

		if (&PageTable::current() == &m_page_table || &PageTable::kernel() == &m_page_table)
			memset(reinterpret_cast<void*>(vaddr()), 0, size());
		else
		{
			const size_t page_count = size() / PAGE_SIZE;
			for (size_t i = 0; i < page_count; i++)
			{
				PageTable::with_fast_page(m_paddrs[i], [&] {
					memset(PageTable::fast_page_as_ptr(), 0, PAGE_SIZE);
				});
			}
		}

		return {};
	}

	BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> VirtualRange::clone(PageTable& page_table)
	{
		ASSERT(&PageTable::current() == &m_page_table);
		ASSERT(&m_page_table != &page_table);

		SpinLockGuard _(m_lock);

		auto result = TRY(create_to_vaddr(page_table, vaddr(), size(), m_flags, m_preallocated, m_has_guard_pages));

		const size_t page_count = size() / PAGE_SIZE;
		for (size_t i = 0; i < page_count; i++)
		{
			if (m_paddrs[i] == 0)
				continue;
			if (!result->m_preallocated)
			{
				result->m_paddrs[i] = Heap::get().take_free_page();
				if (result->m_paddrs[i] == 0)
					return BAN::Error::from_errno(ENOMEM);
				result->m_page_table.map_page_at(result->m_paddrs[i], vaddr() + i * PAGE_SIZE, m_flags);
			}

			PageTable::with_fast_page(result->m_paddrs[i], [&] {
				memcpy(PageTable::fast_page_as_ptr(), reinterpret_cast<void*>(vaddr() + i * PAGE_SIZE), PAGE_SIZE);
			});
		}

		return result;
	}

	BAN::ErrorOr<bool> VirtualRange::allocate_page_for_demand_paging(vaddr_t vaddr)
	{
		ASSERT(contains(vaddr));
		ASSERT(&PageTable::current() == &m_page_table);

		if (m_preallocated)
			return false;

		const size_t index = (vaddr - this->vaddr()) / PAGE_SIZE;
		if (m_paddrs[index])
			return false;

		SpinLockGuard _(m_lock);

		m_paddrs[index] = Heap::get().take_free_page();
		if (m_paddrs[index] == 0)
			return BAN::Error::from_errno(ENOMEM);

		m_page_table.map_page_at(m_paddrs[index], vaddr, m_flags);
		memset(reinterpret_cast<void*>(vaddr), 0, PAGE_SIZE);

		return true;
	}

}
