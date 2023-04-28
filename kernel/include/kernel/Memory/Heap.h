#pragma once

#include <BAN/NoCopyMove.h>
#include <BAN/Vector.h>

#include <stdint.h>

#define PAGE_SIZE 4096

namespace Kernel
{

	using vaddr_t = uintptr_t;
	using paddr_t = uintptr_t;

	class PhysicalRange
	{
	public:
		static constexpr paddr_t invalid = ~paddr_t(0);

	public:
		PhysicalRange(paddr_t, size_t);
		paddr_t reserve_page();
		void release_page(paddr_t);

		paddr_t start() const { return m_start; }
		paddr_t end() const { return m_start + m_size; }
		bool contains(paddr_t addr) const { return m_start <= addr && addr < m_start + m_size; }

		size_t usable_memory() const { return m_reservable_pages * PAGE_SIZE; }

	private:
		struct node
		{
			node* next;
			node* prev;
		};
		
		paddr_t page_address(const node*) const;
		node* node_address(paddr_t) const;

	private:
		paddr_t m_start	{ 0 };
		size_t m_size	{ 0 };

		uint64_t m_total_pages		{ 0 };
		uint64_t m_reservable_pages	{ 0 };
		uint64_t m_list_pages		{ 0 };

		node* m_free_list { nullptr };
		node* m_used_list { nullptr };
	};
	
	class Heap
	{
		BAN_NON_COPYABLE(Heap);
		BAN_NON_MOVABLE(Heap);

	public:
		static void initialize();
		static Heap& get();

		paddr_t take_free_page();
		void release_page(paddr_t);

	private:
		Heap() = default;
		void initialize_impl();

	private:
		BAN::Vector<PhysicalRange> m_physical_ranges;
	};

}
