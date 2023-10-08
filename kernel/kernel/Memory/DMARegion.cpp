#include <BAN/ScopeGuard.h>
#include <kernel/Memory/DMARegion.h>
#include <kernel/Memory/Heap.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::UniqPtr<DMARegion>> DMARegion::create(size_t size)
	{
		size_t needed_pages = BAN::Math::div_round_up<size_t>(size, PAGE_SIZE);

		vaddr_t vaddr = PageTable::kernel().reserve_free_contiguous_pages(needed_pages, KERNEL_OFFSET);
		if (vaddr == 0)
			return BAN::Error::from_errno(ENOMEM);
		BAN::ScopeGuard vaddr_guard([vaddr, size] { PageTable::kernel().unmap_range(vaddr, size); });

		paddr_t paddr = Heap::get().take_free_contiguous_pages(needed_pages);
		if (paddr == 0)
			return BAN::Error::from_errno(ENOMEM);
		BAN::ScopeGuard paddr_guard([paddr, needed_pages] { Heap::get().release_contiguous_pages(paddr, needed_pages); });

		auto* region_ptr = new DMARegion(size, vaddr, paddr);
		if (region_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		
		vaddr_guard.disable();
		paddr_guard.disable();

		PageTable::kernel().map_range_at(paddr, vaddr, size, PageTable::Flags::CacheDisable | PageTable::Flags::ReadWrite | PageTable::Flags::Present);

		return BAN::UniqPtr<DMARegion>::adopt(region_ptr);
	}

	DMARegion::DMARegion(size_t size, vaddr_t vaddr, paddr_t paddr)
		: m_size(size)
		, m_vaddr(vaddr)
		, m_paddr(paddr)
	{ }

	DMARegion::~DMARegion()
	{
		PageTable::kernel().unmap_range(m_vaddr, m_size);
		Heap::get().release_contiguous_pages(m_paddr, BAN::Math::div_round_up<size_t>(m_size, PAGE_SIZE));
	}

}
