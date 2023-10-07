#include <kernel/LockGuard.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/multiboot.h>

extern uint8_t g_kernel_end[];

namespace Kernel
{

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
			multiboot_memory_map_t* mmmt = (multiboot_memory_map_t*)P2V(g_multiboot_info->mmap_addr + i);
			if (mmmt->type == 1)
			{				
				paddr_t start = mmmt->base_addr;
				if (start < V2P(g_kernel_end))
					start = V2P(g_kernel_end);
				if (auto rem = start % PAGE_SIZE)
					start += PAGE_SIZE - rem;

				paddr_t end = mmmt->base_addr + mmmt->length;
				if (auto rem = end % PAGE_SIZE)
					end -= rem;

				// Physical pages needs atleast 2 pages
				if (end > start + PAGE_SIZE)
					MUST(m_physical_ranges.emplace_back(start, end - start));
			}
			i += mmmt->size + sizeof(uint32_t);
		}

		size_t total = 0;
		for (auto& range : m_physical_ranges)
		{
			size_t bytes = range.usable_memory();
			dprintln("RAM {8H}->{8H} ({}.{} MB)", range.start(), range.end(), bytes / (1 << 20), bytes % (1 << 20) * 1000 / (1 << 20));
			total += bytes;
		}
		dprintln("Total RAM {}.{} MB", total / (1 << 20), total % (1 << 20) * 1000 / (1 << 20));
	}

	paddr_t Heap::take_free_page()
	{
		LockGuard _(m_lock);
		for (auto& range : m_physical_ranges)
			if (range.free_pages() >= 1)
				return range.reserve_page();
		return 0;
	}

	void Heap::release_page(paddr_t paddr)
	{
		LockGuard _(m_lock);
		for (auto& range : m_physical_ranges)
			if (range.contains(paddr))
				return range.release_page(paddr);
		ASSERT_NOT_REACHED();
	}

	paddr_t Heap::take_free_contiguous_pages(size_t pages)
	{
		LockGuard _(m_lock);
		for (auto& range : m_physical_ranges)
			if (range.free_pages() >= pages)
				if (paddr_t paddr = range.reserve_contiguous_pages(pages))
					return paddr;
		return 0;
	}

	void Heap::release_contiguous_pages(paddr_t paddr, size_t pages)
	{
		LockGuard _(m_lock);
		for (auto& range : m_physical_ranges)
			if (range.contains(paddr))
				return range.release_contiguous_pages(paddr, pages);
		ASSERT_NOT_REACHED();
	}

	size_t Heap::used_pages() const
	{
		LockGuard _(m_lock);
		size_t result = 0;
		for (const auto& range : m_physical_ranges)
			result += range.used_pages();
		return result;
	}

	size_t Heap::free_pages() const
	{
		LockGuard _(m_lock);
		size_t result = 0;
		for (const auto& range : m_physical_ranges)
			result += range.free_pages();
		return result;
	}

}
