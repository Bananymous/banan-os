#pragma once

#include <BAN/UniqPtr.h>
#include <kernel/Memory/PageTable.h>

namespace Kernel
{

	class VirtualRange
	{
	public:
		static BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> create_to_vaddr_range(PageTable&, AddressRange, size_t, PageTable::flags_t, bool);
		~VirtualRange();

		vaddr_t vaddr() const { return m_vaddr; }
		size_t size() const { return m_size; }

	private:
		VirtualRange(vaddr_t, size_t);

	private:
		const vaddr_t m_vaddr;
		const size_t m_size;
	};

}
