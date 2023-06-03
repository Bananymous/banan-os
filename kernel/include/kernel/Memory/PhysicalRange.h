#pragma once

#include <kernel/Memory/Types.h>

#include <stddef.h>
#include <stdint.h>

namespace Kernel
{

	class PhysicalRange
	{
	public:
		PhysicalRange(paddr_t, size_t);
		paddr_t reserve_page();
		void release_page(paddr_t);

		paddr_t start() const { return m_paddr; }
		paddr_t end() const { return m_paddr + m_size; }
		bool contains(paddr_t addr) const { return m_paddr <= addr && addr < m_paddr + m_size; }

		size_t usable_memory() const { return m_reservable_pages * PAGE_SIZE; }

		size_t used_pages() const { return m_used_pages; }
		size_t free_pages() const { return m_free_pages; }

	private:
		struct node
		{
			node* next;
			node* prev;
		};
		
		paddr_t page_address(const node*) const;
		node* node_address(paddr_t) const;

	private:
		paddr_t m_paddr	{ 0 };
		vaddr_t m_vaddr { 0 };
		size_t m_size	{ 0 };

		uint64_t m_total_pages		{ 0 };
		uint64_t m_reservable_pages	{ 0 };
		uint64_t m_list_pages		{ 0 };

		size_t m_used_pages { 0 };
		size_t m_free_pages { 0 };

		node* m_free_list { nullptr };
		node* m_used_list { nullptr };
	};

}