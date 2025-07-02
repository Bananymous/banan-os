#pragma once

#include <BAN/NoCopyMove.h>
#include <BAN/UniqPtr.h>
#include <BAN/Vector.h>
#include <kernel/Memory/PageTable.h>

namespace Kernel
{

	class VirtualRange
	{
		BAN_NON_COPYABLE(VirtualRange);
		BAN_NON_MOVABLE(VirtualRange);

	public:
		// Create virtual range to fixed virtual address
		static BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> create_to_vaddr(PageTable&, vaddr_t, size_t, PageTable::flags_t flags, bool preallocate_pages, bool add_guard_pages);
		// Create virtual range to virtual address range
		static BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> create_to_vaddr_range(PageTable&, vaddr_t vaddr_start, vaddr_t vaddr_end, size_t, PageTable::flags_t flags, bool preallocate_pages, bool add_guard_pages);
		~VirtualRange();

		BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> clone(PageTable&);

		vaddr_t vaddr() const { return m_vaddr + (m_has_guard_pages ? PAGE_SIZE : 0); }
		size_t size() const { return m_size - (m_has_guard_pages ? 2 * PAGE_SIZE : 0); }
		PageTable::flags_t flags() const { return m_flags; }

		paddr_t paddr_of(vaddr_t vaddr) const
		{
			ASSERT(vaddr % PAGE_SIZE == 0);
			const size_t index = (vaddr - this->vaddr()) / PAGE_SIZE;
			ASSERT(index < m_paddrs.size());
			const paddr_t paddr = m_paddrs[index];
			ASSERT(paddr);
			return paddr;
		}

		bool contains(vaddr_t address) const { return vaddr() <= address && address < vaddr() + size(); }

		BAN::ErrorOr<void> allocate_page_for_demand_paging(vaddr_t address);

	private:
		VirtualRange(PageTable&, bool preallocated, bool has_guard_pages, vaddr_t, size_t, PageTable::flags_t);
		BAN::ErrorOr<void> initialize();

	private:
		PageTable&               m_page_table;
		const bool               m_preallocated;
		const bool               m_has_guard_pages;
		const vaddr_t            m_vaddr;
		const size_t             m_size;
		const PageTable::flags_t m_flags;
		BAN::Vector<paddr_t>     m_paddrs;
		SpinLock                 m_lock;

		friend class BAN::UniqPtr<VirtualRange>;
	};

}
