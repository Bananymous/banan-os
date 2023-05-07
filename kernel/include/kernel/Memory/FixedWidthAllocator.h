#pragma once

#include <kernel/Memory/Heap.h>

namespace Kernel
{

	class Process;

	class FixedWidthAllocator
	{
		BAN_NON_COPYABLE(FixedWidthAllocator);

	public:
		FixedWidthAllocator(Process*, uint32_t);
		FixedWidthAllocator(FixedWidthAllocator&&);
		~FixedWidthAllocator();

		vaddr_t allocate();
		bool deallocate(vaddr_t);

		uint32_t allocation_size() const { return m_allocation_size; }

		uint32_t allocations() const { return m_allocations; }
		uint32_t max_allocations() const;

	private:
		struct node
		{
			node* prev { nullptr };
			node* next { nullptr };
			bool allocated { false };
		};
		vaddr_t address_of_node(const node*) const;
		node* node_from_address(vaddr_t) const;
		void allocate_page_for_node_if_needed(const node*);

	private:
		static constexpr uint32_t m_min_allocation_size = 16;

		Process* m_process;
		const uint32_t m_allocation_size;
		
		vaddr_t m_nodes_page { 0 };
		vaddr_t m_allocated_pages { 0 };

		node* m_free_list { nullptr };
		node* m_used_list { nullptr };

		uint32_t m_allocations { 0 };
	};

}