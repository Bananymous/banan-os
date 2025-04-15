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
		static BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> create_to_vaddr(PageTable&, vaddr_t, size_t, PageTable::flags_t flags, bool preallocate_pages);
		// Create virtual range to virtual address range
		static BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> create_to_vaddr_range(PageTable&, vaddr_t vaddr_start, vaddr_t vaddr_end, size_t, PageTable::flags_t flags, bool preallocate_pages);
		~VirtualRange();

		BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> clone(PageTable&);

		vaddr_t vaddr() const { return m_vaddr; }
		size_t size() const { return m_size; }
		PageTable::flags_t flags() const { return m_flags; }

		paddr_t paddr_of(vaddr_t vaddr) const
		{
			ASSERT(vaddr % PAGE_SIZE == 0);
			const size_t index = (vaddr - m_vaddr) / PAGE_SIZE;
			ASSERT(index < m_paddrs.size());
			const paddr_t paddr = m_paddrs[index];
			ASSERT(paddr);
			return paddr;
		}

		bool contains(vaddr_t address) const { return vaddr() <= address && address < vaddr() + size(); }

		BAN::ErrorOr<void> allocate_page_for_demand_paging(vaddr_t address);

	private:
		VirtualRange(PageTable&, bool preallocated, vaddr_t, size_t, PageTable::flags_t);
		BAN::ErrorOr<void> initialize();

	private:
		PageTable&               m_page_table;
		const bool               m_preallocated;
		const vaddr_t            m_vaddr;
		const size_t             m_size;
		const PageTable::flags_t m_flags;
		BAN::Vector<paddr_t>     m_paddrs;
		SpinLock                 m_lock;

		friend class BAN::UniqPtr<VirtualRange>;
	};

}
