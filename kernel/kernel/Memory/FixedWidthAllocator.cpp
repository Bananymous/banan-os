#include <kernel/CriticalScope.h>
#include <kernel/Memory/FixedWidthAllocator.h>
#include <kernel/Memory/MMU.h>
#include <kernel/Process.h>

namespace Kernel
{

	FixedWidthAllocator::FixedWidthAllocator(Process* process, uint32_t allocation_size)
		: m_process(process)
		, m_allocation_size(BAN::Math::max(allocation_size, m_min_allocation_size))
	{
		ASSERT(BAN::Math::is_power_of_two(allocation_size));

		paddr_t nodes_paddr = Heap::get().take_free_page();
		m_nodes_page = m_process->mmu().get_free_page();
		m_process->mmu().map_page_at(nodes_paddr, m_nodes_page, MMU::Flags::ReadWrite | MMU::Flags::Present);

		paddr_t allocated_pages_paddr = Heap::get().take_free_page();
		m_allocated_pages = m_process->mmu().get_free_page();
		m_process->mmu().map_page_at(allocated_pages_paddr, m_allocated_pages, MMU::Flags::ReadWrite | MMU::Flags::Present);

		CriticalScope _;

		m_process->mmu().load();

		memset((void*)m_nodes_page, 0, PAGE_SIZE);
		memset((void*)m_allocated_pages, 0, PAGE_SIZE);

		node* node_table = (node*)m_nodes_page;
		for (uint32_t i = 0; i < PAGE_SIZE / sizeof(node); i++)
		{
			node_table[i].next = &node_table[i + 1];
			node_table[i].prev = &node_table[i - 1];
		}
		node_table[0].prev = nullptr;
		node_table[PAGE_SIZE / sizeof(node) - 1].next = nullptr;

		m_free_list = node_table;
		m_used_list = nullptr;

		Process::current().mmu().load();
	}

	FixedWidthAllocator::FixedWidthAllocator(FixedWidthAllocator&& other)
		: m_process(other.m_process)
		, m_allocation_size(other.m_allocation_size)
		, m_nodes_page(other.m_nodes_page)
		, m_allocated_pages(other.m_allocated_pages)
		, m_free_list(other.m_free_list)
		, m_used_list(other.m_used_list)
		, m_allocated(other.m_allocated)
	{
		other.m_process = nullptr;
	}

	FixedWidthAllocator::~FixedWidthAllocator()
	{
		if (m_process == nullptr)
			return;
		
		Heap::get().release_page(m_process->mmu().physical_address_of(m_nodes_page));
		m_process->mmu().unmap_page(m_nodes_page);

		for (uint32_t page_index = 0; page_index < PAGE_SIZE / sizeof(vaddr_t); page_index++)
		{
			vaddr_t page_vaddr = ((vaddr_t*)m_allocated_pages)[page_index];
			if (page_vaddr == 0)
				continue;

			ASSERT(!m_process->mmu().is_page_free(page_vaddr));
			paddr_t page_paddr = m_process->mmu().physical_address_of(page_vaddr);

			Heap::get().release_page(page_paddr);
			m_process->mmu().unmap_page(page_vaddr);
		}

		Heap::get().release_page(m_process->mmu().physical_address_of(m_allocated_pages));
		m_process->mmu().unmap_page(m_allocated_pages);
	}

	paddr_t FixedWidthAllocator::allocate()
	{
		// FIXME: We should get allocate more memory if we run out of
		//        nodes in free list.
		ASSERT(m_free_list);

		node* node = m_free_list;

		m_free_list = node->next;
		if (m_free_list)
			m_free_list->prev = nullptr;

		node->next = m_used_list;
		node->prev = nullptr;

		if (m_used_list)
			m_used_list->prev = node;
		m_used_list = node;

		m_allocated++;
		allocate_page_for_node_if_needed(node);
		return address_of(node);
	}

	void FixedWidthAllocator::deallocate(paddr_t addr)
	{
		(void)addr;
		ASSERT_NOT_REACHED();
	}

	vaddr_t FixedWidthAllocator::address_of(const node* node) const
	{
		uint32_t index = node - (struct node*)m_nodes_page;

		uint32_t page_index = index / (PAGE_SIZE / sizeof(struct node));
		ASSERT(page_index < PAGE_SIZE / sizeof(vaddr_t));

		uint32_t offset = index % (PAGE_SIZE / sizeof(struct node));

		vaddr_t page_begin = ((vaddr_t*)m_allocated_pages)[page_index];
		ASSERT(page_begin);

		return page_begin + offset * m_allocation_size;
	}

	void FixedWidthAllocator::allocate_page_for_node_if_needed(const node* node)
	{
		uint32_t index = node - (struct node*)m_nodes_page;

		uint32_t page_index = index / (PAGE_SIZE / sizeof(struct node));
		ASSERT(page_index < PAGE_SIZE / sizeof(vaddr_t));

		vaddr_t& page_vaddr = ((vaddr_t*)m_allocated_pages)[page_index];
		if (page_vaddr)
			return;
		
		paddr_t page_paddr = Heap::get().take_free_page();
		ASSERT(page_paddr);

		page_vaddr = m_process->mmu().get_free_page();
		m_process->mmu().map_page_at(page_paddr, page_vaddr, MMU::Flags::UserSupervisor | MMU::Flags::ReadWrite | MMU::Flags::Present);
	}

}