#include <kernel/Memory/Heap.h>
#include <kernel/Memory/PageTableScope.h>
#include <kernel/Memory/VirtualRange.h>

namespace Kernel
{

	VirtualRange* VirtualRange::create(PageTable& page_table, vaddr_t vaddr, size_t size, uint8_t flags)
	{
		ASSERT(size % PAGE_SIZE == 0);
		ASSERT(vaddr % PAGE_SIZE == 0);
		ASSERT(&page_table != &PageTable::kernel());

		VirtualRange* result = new VirtualRange(page_table);
		ASSERT(result);

		result->m_size = size;
		result->m_flags = flags;
		MUST(result->m_physical_pages.reserve(size / PAGE_SIZE));

		page_table.lock();

		if (vaddr == 0)
		{
			vaddr = page_table.get_free_contiguous_pages(size / PAGE_SIZE);
			ASSERT(vaddr);
		}

		result->m_vaddr = vaddr;

		ASSERT(page_table.is_range_free(vaddr, size));
		for (size_t offset = 0; offset < size; offset += PAGE_SIZE)
		{
			paddr_t paddr = Heap::get().take_free_page();
			ASSERT(paddr);
			MUST(result->m_physical_pages.push_back(paddr));
			page_table.map_page_at(paddr, vaddr + offset, flags);
		}
		page_table.unlock();

		return result;
	}

	VirtualRange* VirtualRange::create_kmalloc(size_t size)
	{
		VirtualRange* result = new VirtualRange(PageTable::kernel());
		if (result == nullptr)
			return nullptr;
		result->m_size = size;
		result->m_flags = PageTable::Flags::ReadWrite | PageTable::Flags::Present;
		result->m_vaddr = (vaddr_t)kmalloc(size);
		if (result->m_vaddr == 0)
		{
			delete result;
			return nullptr;
		}
		return result;
	}

	VirtualRange::VirtualRange(PageTable& page_table)
		: m_page_table(page_table)
	{ }

	VirtualRange::~VirtualRange()
	{
		if (&m_page_table == &PageTable::kernel())
		{
			kfree((void*)m_vaddr);
			return;
		}

		m_page_table.unmap_range(vaddr(), size());
		for (paddr_t page : m_physical_pages)
			Heap::get().release_page(page);
	}

	VirtualRange* VirtualRange::clone(PageTable& page_table)
	{
		VirtualRange* result = create(page_table, vaddr(), size(), flags());

		PageTableScope _(m_page_table);
		ASSERT(m_page_table.is_page_free(0));
		for (size_t i = 0; i < result->m_physical_pages.size(); i++)
		{
			m_page_table.map_page_at(result->m_physical_pages[i], 0, PageTable::Flags::ReadWrite | PageTable::Flags::Present);
			m_page_table.invalidate(0);
			memcpy((void*)0, (void*)(vaddr() + i * PAGE_SIZE), PAGE_SIZE);
		}
		m_page_table.unmap_page(0);
		m_page_table.invalidate(0);

		return result;
	}

}