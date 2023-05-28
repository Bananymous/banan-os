#include <BAN/Assert.h>
#include <BAN/Math.h>
#include <kernel/Memory/MMU.h>
#include <kernel/Memory/PhysicalRange.h>

extern uint8_t g_kernel_end[];

namespace Kernel
{

	PhysicalRange::PhysicalRange(paddr_t start, size_t size)
	{
		// We can't use the memory ovelapping with kernel
		if (start + size <= (paddr_t)g_kernel_end)
			return;

		// Align start to page boundary and after the kernel memory
		m_start = BAN::Math::max(start, (paddr_t)g_kernel_end);
		if (auto rem = m_start % PAGE_SIZE)
			m_start += PAGE_SIZE - rem;

		if (size <= m_start - start)
			return;

		// Align size to page boundary
		m_size = size - (m_start - start);
		if (auto rem = m_size % PAGE_SIZE)
			m_size -= rem;

		// We need atleast 2 pages
		m_total_pages = m_size / PAGE_SIZE;
		if (m_total_pages <= 1)
			return;

		// FIXME: if total pages is just over multiple of (PAGE_SIZE / sizeof(node)) we might make
		//        couple of pages unallocatable
		m_list_pages		= BAN::Math::div_round_up<uint64_t>(m_total_pages * sizeof(node), PAGE_SIZE);
		m_reservable_pages	= m_total_pages - m_list_pages;

		MMU::kernel().identity_map_range(m_start, m_list_pages * PAGE_SIZE, MMU::Flags::ReadWrite | MMU::Flags::Present);

		// Initialize page list so that every page points to the next one
		node* page_list = (node*)m_start;

		ASSERT((paddr_t)&page_list[m_reservable_pages - 1] <= m_start + m_size);

		for (uint64_t i = 0; i < m_reservable_pages; i++)
			page_list[i] = { page_list + i - 1, page_list + i + 1 };
		page_list[           0          ].next = nullptr;
		page_list[m_reservable_pages - 1].prev = nullptr;

		m_free_list = page_list;
		m_used_list = nullptr;
	}

	paddr_t PhysicalRange::reserve_page()
	{
		if (m_free_list == nullptr)
			return 0;

		node* page = m_free_list;
		ASSERT(page->next == nullptr);

		// Detatch page from top of the free list
		m_free_list = m_free_list->prev ? m_free_list->prev : nullptr;
		if (m_free_list)
			m_free_list->next = nullptr;

		// Add page to used list
		if (m_used_list)
			m_used_list->next = page;
		page->prev = m_used_list;
		m_used_list = page;

		return page_address(page);
	}

	void PhysicalRange::release_page(paddr_t page_address)
	{
		ASSERT(m_used_list);

		node* page = node_address(page_address);
		
		// Detach page from used list
		if (page->prev)
			page->prev->next = page->next;
		if (page->next)
			page->next->prev = page->prev;
		if (m_used_list == page)
			m_used_list = page->prev;

		// Add page to the top of free list
		page->prev = m_free_list;
		page->next = nullptr;
		if (m_free_list)
			m_free_list->next = page;
		m_free_list = page;
	}	

	paddr_t PhysicalRange::page_address(const node* page) const
	{
		ASSERT((paddr_t)page <= m_start + m_reservable_pages * sizeof(node));
		uint64_t page_index = page - (node*)m_start;
		return m_start + (page_index + m_list_pages) * PAGE_SIZE;
	}

	PhysicalRange::node* PhysicalRange::node_address(paddr_t page_address) const
	{
		ASSERT(page_address % PAGE_SIZE == 0);
		ASSERT(m_start + m_list_pages * PAGE_SIZE <= page_address && page_address < m_start + m_size);
		uint64_t page_offset = page_address - (m_start + m_list_pages * PAGE_SIZE);
		return (node*)m_start + page_offset / PAGE_SIZE;
	}	

}
