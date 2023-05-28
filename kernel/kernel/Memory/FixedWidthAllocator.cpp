#include <kernel/Memory/FixedWidthAllocator.h>
#include <kernel/Memory/MMUScope.h>

namespace Kernel
{

	FixedWidthAllocator::FixedWidthAllocator(MMU& mmu, uint32_t allocation_size)
		: m_mmu(mmu)
		, m_allocation_size(BAN::Math::max(allocation_size, m_min_allocation_size))
	{
		ASSERT(BAN::Math::is_power_of_two(allocation_size));

		m_nodes_page = (vaddr_t)kmalloc(PAGE_SIZE);
		ASSERT(m_nodes_page);

		m_allocated_pages = (vaddr_t)kmalloc(PAGE_SIZE);
		ASSERT(m_allocated_pages);

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
	}

	FixedWidthAllocator::~FixedWidthAllocator()
	{
		for (uint32_t page_index = 0; page_index < PAGE_SIZE / sizeof(vaddr_t); page_index++)
		{
			vaddr_t page_vaddr = ((vaddr_t*)m_allocated_pages)[page_index];
			if (page_vaddr == 0)
				continue;

			ASSERT(!m_mmu.is_page_free(page_vaddr));
			Heap::get().release_page(m_mmu.physical_address_of(page_vaddr));
			m_mmu.unmap_page(page_vaddr);
		}

		kfree((void*)m_nodes_page);
		kfree((void*)m_allocated_pages);
	}

	paddr_t FixedWidthAllocator::allocate()
	{
		if (m_free_list == nullptr)
			return 0;
		node* node = m_free_list;
		allocate_node(node);
		allocate_page_for_node_if_needed(node);
		return address_of_node(node);
	}

	bool FixedWidthAllocator::deallocate(vaddr_t address)
	{
		if (address % m_allocation_size)
			return false;
		if (m_allocations == 0)
			return false;
		
		node* node = node_from_address(address);
		if (node == nullptr)
			return false;

		if (!node->allocated)
		{
			dwarnln("deallocate called on unallocated address");
			return true;
		}

		deallocate_node(node);
		return true;
	}

	void FixedWidthAllocator::allocate_node(node* node)
	{
		ASSERT(!node->allocated);
		node->allocated = true;

		if (node == m_free_list)
			m_free_list = node->next;

		if (node->prev)
			node->prev->next = node->next;
		if (node->next)
			node->next->prev = node->prev;

		node->next = m_used_list;
		node->prev = nullptr;

		if (m_used_list)
			m_used_list->prev = node;
		m_used_list = node;

		m_allocations++;
	}

	void FixedWidthAllocator::deallocate_node(node* node)
	{
		ASSERT(node->allocated);
		node->allocated = false;

		if (node == m_used_list)
			m_used_list = node->next;

		if (node->prev)
			node->prev->next = node->next;
		if (node->next)
			node->next->prev = node->prev;

		node->next = m_free_list;
		node->prev = nullptr;

		if (m_free_list)
			m_free_list->prev = node;
		m_free_list = node;

		m_allocations--;
	}

	uint32_t FixedWidthAllocator::max_allocations() const
	{
		return PAGE_SIZE / sizeof(node);
	}

	vaddr_t FixedWidthAllocator::address_of_node(const node* node) const
	{
		uint32_t index = node - (struct node*)m_nodes_page;

		uint32_t page_index = index / (PAGE_SIZE / m_allocation_size);
		ASSERT(page_index < PAGE_SIZE / sizeof(vaddr_t));

		uint32_t offset = index % (PAGE_SIZE / m_allocation_size);

		vaddr_t page_begin = ((vaddr_t*)m_allocated_pages)[page_index];
		ASSERT(page_begin);

		return page_begin + offset * m_allocation_size;
	}

	FixedWidthAllocator::node* FixedWidthAllocator::node_from_address(vaddr_t address) const
	{
		// TODO: This probably should be optimized from O(n) preferably to O(1) but I
		//       don't want to think about performance now.
		
		ASSERT(address % m_allocation_size == 0);

		vaddr_t page_begin = address / PAGE_SIZE * PAGE_SIZE;

		for (uint32_t page_index = 0; page_index < PAGE_SIZE / sizeof(vaddr_t); page_index++)
		{
			vaddr_t vaddr = ((vaddr_t*)m_allocated_pages)[page_index];
			if (vaddr != page_begin)
				continue;
			
			uint32_t offset = (address - page_begin) / m_allocation_size;

			node* result = (node*)m_nodes_page;
			result += page_index * PAGE_SIZE / m_allocation_size;
			result += offset;
			ASSERT(address_of_node(result) == address);
			return result;
		}

		return nullptr;
	}

	void FixedWidthAllocator::allocate_page_for_node_if_needed(const node* node)
	{
		uint32_t index = node - (struct node*)m_nodes_page;

		uint32_t page_index = index / (PAGE_SIZE / m_allocation_size);
		ASSERT(page_index < PAGE_SIZE / sizeof(vaddr_t));

		vaddr_t& page_vaddr = ((vaddr_t*)m_allocated_pages)[page_index];
		if (page_vaddr)
			return;
		
		paddr_t page_paddr = Heap::get().take_free_page();
		ASSERT(page_paddr);

		page_vaddr = m_mmu.get_free_page();
		m_mmu.map_page_at(page_paddr, page_vaddr, MMU::Flags::UserSupervisor | MMU::Flags::ReadWrite | MMU::Flags::Present);
	}

	bool FixedWidthAllocator::allocate_page_if_needed(vaddr_t vaddr, uint8_t flags)
	{
		ASSERT(vaddr % PAGE_SIZE == 0);

		// Check if page is already allocated
		for (uint32_t page_index = 0; page_index < PAGE_SIZE / sizeof(vaddr_t); page_index++)
		{
			vaddr_t page_begin = ((vaddr_t*)m_allocated_pages)[page_index];
			if (vaddr == page_begin)
				return false;
		}

		// Page is not allocated so the vaddr must not be in use
		ASSERT(m_mmu.is_page_free(vaddr));

		// Allocate the vaddr on empty page
		for (uint32_t page_index = 0; page_index < PAGE_SIZE / sizeof(vaddr_t); page_index++)
		{
			vaddr_t& page_begin = ((vaddr_t*)m_allocated_pages)[page_index];
			if (page_begin == 0)
			{
				paddr_t paddr = Heap::get().take_free_page();
				ASSERT(paddr);
				m_mmu.map_page_at(paddr, vaddr, flags);
				page_begin = vaddr;
				return true;
			}
		}

		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<FixedWidthAllocator*> FixedWidthAllocator::clone(MMU& new_mmu)
	{
		FixedWidthAllocator* allocator = new FixedWidthAllocator(new_mmu, allocation_size());
		if (allocator == nullptr)
			return BAN::Error::from_errno(ENOMEM);

		MMUScope _(m_mmu);
		ASSERT(m_mmu.is_page_free(0));

		for (node* node = m_used_list; node; node = node->next)
		{
			ASSERT(node->allocated);

			vaddr_t vaddr = address_of_node(node);
			vaddr_t page_begin = vaddr & PAGE_ADDR_MASK;
			uint8_t flags = m_mmu.get_page_flags(page_begin);

			// Allocate and copy all data from this allocation to the new one
			if (allocator->allocate_page_if_needed(page_begin, flags))
			{
				paddr_t paddr = new_mmu.physical_address_of(page_begin);
				m_mmu.map_page_at(paddr, 0, MMU::Flags::ReadWrite | MMU::Flags::Present);
				m_mmu.invalidate(0);
				memcpy((void*)0, (void*)page_begin, PAGE_SIZE);
			}

			// Now that we are sure the page is allocated, we can access the node
			struct node* new_node = allocator->node_from_address(vaddr);
			allocator->allocate_node(new_node);
		}

		m_mmu.unmap_page(0);
		m_mmu.invalidate(0);

		return allocator;
	}

}