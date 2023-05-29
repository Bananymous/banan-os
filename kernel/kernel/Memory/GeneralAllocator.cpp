#include <kernel/Memory/GeneralAllocator.h>

namespace Kernel
{

	GeneralAllocator::GeneralAllocator(PageTable& page_table)
		: m_page_table(page_table)
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

		allocation.address = m_page_table.get_free_contiguous_pages(needed_pages);
		for (size_t i = 0; i < needed_pages; i++)
			m_page_table.map_page_at(allocation.pages[i], allocation.address + i * PAGE_SIZE, PageTable::Flags::UserSupervisor | PageTable::Flags::ReadWrite | PageTable::Flags::Present);

		MUST(m_allocations.push_back(BAN::move(allocation)));
		return allocation.address;
	}

	bool GeneralAllocator::deallocate(vaddr_t address)
	{
		for (auto it = m_allocations.begin(); it != m_allocations.end(); it++)
		{
			if (it->address != address)
				continue;
			
			m_page_table.unmap_range(it->address, it->pages.size() * PAGE_SIZE);
			for (auto paddr : it->pages)
				Heap::get().release_page(paddr);

			m_allocations.remove(it);

			return true;
		}

		return false;
	}

	BAN::ErrorOr<GeneralAllocator*> GeneralAllocator::clone(PageTable& new_page_table)
	{
		GeneralAllocator* allocator = new GeneralAllocator(new_page_table);
		if (allocator == nullptr)
			return BAN::Error::from_errno(ENOMEM);

		m_page_table.lock();

		ASSERT(m_page_table.is_page_free(0));

		for (auto& allocation : m_allocations)
		{
			Allocation new_allocation;
			ASSERT(new_page_table.is_range_free(allocation.address, allocation.pages.size() * PAGE_SIZE));

			new_allocation.address = allocation.address;
			MUST(new_allocation.pages.reserve(allocation.pages.size()));

			uint8_t flags = m_page_table.get_page_flags(allocation.address);
			for (size_t i = 0; i < allocation.pages.size(); i++)
			{
				paddr_t paddr = Heap::get().take_free_page();
				ASSERT(paddr);

				vaddr_t vaddr = allocation.address + i * PAGE_SIZE;

				MUST(new_allocation.pages.push_back(paddr));
				new_page_table.map_page_at(paddr, vaddr, flags);

				m_page_table.map_page_at(paddr, 0, PageTable::Flags::ReadWrite | PageTable::Flags::Present);
				m_page_table.invalidate(0);
				memcpy((void*)0, (void*)vaddr, PAGE_SIZE);
			}

			MUST(allocator->m_allocations.push_back(BAN::move(new_allocation)));
		}
		m_page_table.unmap_page(0);
		m_page_table.invalidate(0);

		m_page_table.unlock();

		return allocator;
	}

}