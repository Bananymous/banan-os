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
		paddr_t end() const { return m_paddr + m_size; }
		bool contains(paddr_t addr) const { return m_paddr <= addr && addr < m_paddr + m_size; }

		size_t usable_memory() const { return m_data_pages * PAGE_SIZE; }

		size_t used_pages() const { return m_data_pages - m_free_pages; }
		size_t free_pages() const { return m_free_pages; }

	private:
		unsigned long long* ull_bitmap_ptr() { return (unsigned long long*)m_vaddr; }
		const unsigned long long* ull_bitmap_ptr() const { return (const unsigned long long*)m_vaddr; }
		
		paddr_t paddr_for_bit(unsigned long long) const;
		unsigned long long bit_for_paddr(paddr_t paddr) const;

		unsigned long long contiguous_bits_set(unsigned long long start, unsigned long long count) const;

	private:
		const paddr_t m_paddr { 0 };
		const size_t m_size	{ 0 };

		vaddr_t m_vaddr { 0 };

		const size_t m_bitmap_pages { 0 };
		const size_t m_data_pages { 0 };
		size_t m_free_pages { 0 };
	};

}