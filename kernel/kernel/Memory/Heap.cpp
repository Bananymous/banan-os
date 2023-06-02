#include <kernel/LockGuard.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/multiboot.h>

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
				PhysicalRange range(mmmt->base_addr, mmmt->length);
				if (range.usable_memory() > 0)
					MUST(m_physical_ranges.push_back(range));
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
			if (paddr_t page = range.reserve_page())
				return page;
		return 0;
	}
	
	void Heap::release_page(paddr_t addr)
	{
		LockGuard _(m_lock);
		for (auto& range : m_physical_ranges)
		{
			if (range.contains(addr))
			{
				range.release_page(addr);
				return;
			}
		}
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
