#include <kernel/Memory/Heap.h>
#include <kernel/Memory/MMU.h>
#include <kernel/multiboot.h>

extern uint8_t g_kernel_end[];

namespace Kernel::Memory
{

	PhysicalRange::PhysicalRange(paddr_t start, size_t size)
	{
		ASSERT(start + size > (paddr_t)g_kernel_end);

		// Align start to page boundary and after the kernel memory
		m_start = BAN::Math::max(start, (paddr_t)g_kernel_end);
		if (auto rem = m_start % PAGE_SIZE)
			m_start += PAGE_SIZE - rem;

		// Align size to page boundary
		m_size = size - (m_start - start);
		if (auto rem = m_size % PAGE_SIZE)
			m_size -= rem;

		// FIXME: if total pages is just over multiple of (4096/sizeof(uint64_t)) we might make
		//        couple of pages unallocatable
		m_total_pages		= m_size / PAGE_SIZE;
		m_list_pages		= BAN::Math::div_round_up<uint64_t>(m_total_pages * sizeof(uint64_t), PAGE_SIZE);
		m_reservable_pages	= m_total_pages - m_list_pages;

		MMU::get().allocate_range(m_start, m_list_pages * PAGE_SIZE, MMU::Flags::Present);

		// Initialize free list with every page pointing to the next one
		uint64_t* list_ptr = (uint64_t*)m_start;
		for (uint64_t i = 0; i < m_reservable_pages - 1; i++)
		{
			*list_ptr++ = i + 1;
			//dprintln("{}/{}", i, m_reservable_pages);
		}

		*list_ptr = invalid;
		m_free_list = (uint64_t*)m_start;

		m_used_list = nullptr;
	}

	paddr_t PhysicalRange::reserve_page()
	{
		ASSERT_NOT_REACHED();
	}

	void PhysicalRange::release_page(paddr_t)
	{
		ASSERT_NOT_REACHED();
	}	

	paddr_t PhysicalRange::page_address(uint64_t page_index) const
	{
		ASSERT(page_index < m_reservable_pages);
		return m_start + (page_index + m_list_pages) * PAGE_SIZE;
	}



	static Heap* s_instance = nullptr;

	void Heap::initialize()
	{
		ASSERT(s_instance == nullptr);
		s_instance = new Heap;
		ASSERT(s_instance);
		s_instance->initialize_impl();
	}

	Heap& Heap::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	void Heap::initialize_impl()
	{
		if (!(g_multiboot_info->flags & (1 << 6)))
			Kernel::panic("Bootloader did not provide a memory map");
		
		for (size_t i = 0; i < g_multiboot_info->mmap_length;)
		{
			multiboot_memory_map_t* mmmt = (multiboot_memory_map_t*)(g_multiboot_info->mmap_addr + i);

			if (mmmt->type == 1)
			{
				// We can't use the memory ovelapping with kernel
				if (mmmt->base_addr + mmmt->length > (paddr_t)g_kernel_end)
					MUST(m_physical_ranges.push_back({ mmmt->base_addr, mmmt->length }));
			}

			i += mmmt->size + sizeof(uint32_t);
		}

		for (auto& range : m_physical_ranges)
			dprintln("RAM {8H}->{8H}, {} pages ({}.{} MB)", range.start(), range.end(), range.pages(), range.pages() * PAGE_SIZE / (1 << 20), range.pages() * PAGE_SIZE % (1 << 20) * 100 / (1 << 20));
	}

}
