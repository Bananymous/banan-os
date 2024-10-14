#pragma once

#include <kernel/Memory/Types.h>

#include <stddef.h>

namespace Kernel
{

	class PhysicalRange
	{
	public:
		PhysicalRange(paddr_t, size_t);

		paddr_t reserve_page();
		void release_page(paddr_t);

		paddr_t reserve_contiguous_pages(size_t pages);
		void release_contiguous_pages(paddr_t paddr, size_t pages);

		paddr_t start() const { return m_paddr; }
		paddr_t end() const { return m_paddr + m_page_count * PAGE_SIZE; }
		bool contains(paddr_t addr) const { return start() <= addr && addr < end(); }

		size_t usable_memory() const { return m_page_count * PAGE_SIZE; }

		size_t used_pages() const { return m_page_count - m_free_pages; }
		size_t free_pages() const { return m_free_pages; }

	private:
		const paddr_t m_paddr { 0 };
		const size_t m_page_count { 0 };
		size_t m_free_pages { 0 };
	};

}
