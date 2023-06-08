#pragma once

#include <BAN/Vector.h>
#include <BAN/NoCopyMove.h>
#include <kernel/Memory/PageTable.h>

namespace Kernel
{

	class VirtualRange
	{
		BAN_NON_COPYABLE(VirtualRange);
		BAN_NON_MOVABLE(VirtualRange);

	public:
		static VirtualRange* create(PageTable&, vaddr_t, size_t, uint8_t flags);
		static VirtualRange* create_kmalloc(size_t);
		~VirtualRange();

		VirtualRange* clone(PageTable&);

		vaddr_t vaddr() const { return m_vaddr; }
		size_t size() const { return m_size; }
		uint8_t flags() const { return m_flags; }

		void set_zero();
		void copy_from(size_t offset, const uint8_t* buffer, size_t bytes);

	private:
		VirtualRange(PageTable&);

	private:
		PageTable&				m_page_table;
		bool					m_kmalloc { false };
		vaddr_t					m_vaddr { 0 };
		size_t					m_size { 0 };
		uint8_t					m_flags { 0 };
		BAN::Vector<paddr_t>	m_physical_pages;
	};

}