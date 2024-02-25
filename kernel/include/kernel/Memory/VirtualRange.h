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

		bool contains(vaddr_t address) const { return vaddr() <= address && address < vaddr() + size(); }

		BAN::ErrorOr<void> allocate_page_for_demand_paging(vaddr_t address);

		void copy_from(size_t offset, const uint8_t* buffer, size_t bytes);

	private:
		VirtualRange(PageTable&, bool preallocated);

		void set_zero();

	private:
		PageTable&				m_page_table;
		const bool				m_preallocated;
		vaddr_t					m_vaddr { 0 };
		size_t					m_size { 0 };
		PageTable::flags_t		m_flags { 0 };
	};

}
