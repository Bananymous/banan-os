#include <kernel/Memory/GeneralAllocator.h>
#include <kernel/Process.h>

namespace Kernel
{

	GeneralAllocator::GeneralAllocator(MMU& mmu)
		: m_mmu(mmu)
	{ }

	GeneralAllocator::~GeneralAllocator()
	{
		while (!m_allocations.empty())
			deallocate(m_allocations.front().address);
	}

	vaddr_t GeneralAllocator::allocate(size_t bytes)
	{
		size_t needed_pages = BAN::Math::div_round_up<size_t>(bytes, PAGE_SIZE);

		Allocation allocation;
		if (allocation.pages.resize(needed_pages, 0).is_error())
			return 0;

		for (size_t i = 0; i < needed_pages; i++)
		{
			paddr_t paddr = Heap::get().take_free_page();
			if (paddr == 0)
			{
				for (size_t j = 0; j < i; j++)
					Heap::get().release_page(allocation.pages[j]);
				return 0;
			}
			allocation.pages[i] = paddr;
		}

		allocation.address = m_mmu.get_free_contiguous_pages(needed_pages);
		for (size_t i = 0; i < needed_pages; i++)
			m_mmu.map_page_at(allocation.pages[i], allocation.address + i * PAGE_SIZE, MMU::Flags::UserSupervisor | MMU::Flags::ReadWrite | MMU::Flags::Present);

		MUST(m_allocations.push_back(BAN::move(allocation)));
		return allocation.address;
	}

	bool GeneralAllocator::deallocate(vaddr_t address)
	{
		for (auto it = m_allocations.begin(); it != m_allocations.end(); it++)
		{
			if (it->address != address)
				continue;
			
			m_mmu.unmap_range(it->address, it->pages.size() * PAGE_SIZE);
			for (auto paddr : it->pages)
				Heap::get().release_page(paddr);

			m_allocations.remove(it);

			return true;
		}

		return false;
	}

}