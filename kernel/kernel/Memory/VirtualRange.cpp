#include <kernel/Memory/Heap.h>
#include <kernel/Memory/MMUScope.h>
#include <kernel/Memory/VirtualRange.h>

namespace Kernel
{

	VirtualRange* VirtualRange::create(MMU& mmu, vaddr_t vaddr, size_t size, uint8_t flags)
	{
		ASSERT(size % PAGE_SIZE == 0);
		ASSERT(vaddr % PAGE_SIZE == 0);
		ASSERT(&mmu != &MMU::kernel());

		VirtualRange* result = new VirtualRange(mmu);
		ASSERT(result);

		result->m_size = size;
		result->m_flags = flags;
		MUST(result->m_physical_pages.reserve(size / PAGE_SIZE));

		mmu.lock();

		if (vaddr == 0)
		{
			vaddr = mmu.get_free_contiguous_pages(size / PAGE_SIZE);
			ASSERT(vaddr);
		}

		result->m_vaddr = vaddr;

		ASSERT(mmu.is_range_free(vaddr, size));
		for (size_t offset = 0; offset < size; offset += PAGE_SIZE)
		{
			paddr_t paddr = Heap::get().take_free_page();
			ASSERT(paddr);
			MUST(result->m_physical_pages.push_back(paddr));
			mmu.map_page_at(paddr, vaddr + offset, flags);
		}
		mmu.unlock();

		return result;
	}

	VirtualRange* VirtualRange::create_kmalloc(size_t size)
	{
		VirtualRange* result = new VirtualRange(MMU::kernel());
		if (result == nullptr)
			return nullptr;
		result->m_size = size;
		result->m_flags = MMU::Flags::ReadWrite | MMU::Flags::Present;
		result->m_vaddr = (vaddr_t)kmalloc(size);
		if (result->m_vaddr == 0)
		{
			delete result;
			return nullptr;
		}
		return result;
	}

	VirtualRange::VirtualRange(MMU& mmu)
		: m_mmu(mmu)
	{ }

	VirtualRange::~VirtualRange()
	{
		if (&m_mmu == &MMU::kernel())
		{
			kfree((void*)m_vaddr);
			return;
		}

		m_mmu.unmap_range(vaddr(), size());
		for (paddr_t page : m_physical_pages)
			Heap::get().release_page(page);
	}

	VirtualRange* VirtualRange::clone(MMU& mmu)
	{
		VirtualRange* result = create(mmu, vaddr(), size(), flags());

		MMUScope _(m_mmu);
		ASSERT(m_mmu.is_page_free(0));
		for (size_t i = 0; i < result->m_physical_pages.size(); i++)
		{
			m_mmu.map_page_at(result->m_physical_pages[i], 0, MMU::Flags::ReadWrite | MMU::Flags::Present);
			m_mmu.invalidate(0);
			memcpy((void*)0, (void*)(vaddr() + i * PAGE_SIZE), PAGE_SIZE);
		}
		m_mmu.unmap_page(0);
		m_mmu.invalidate(0);

		return result;
	}

}