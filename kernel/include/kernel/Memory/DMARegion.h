#pragma once

#include <kernel/Memory/MemoryRegion.h>

namespace Kernel
{
	
	class DMARegion
	{
	public:
		static BAN::ErrorOr<BAN::UniqPtr<DMARegion>> create(size_t size);
		~DMARegion();

		size_t size() const { return m_size; }
		vaddr_t vaddr() const { return m_vaddr; }
		paddr_t paddr() const { return m_paddr; }

	private:
		DMARegion(size_t size, vaddr_t vaddr, paddr_t paddr);

	private:
		const size_t m_size;
		const vaddr_t m_vaddr;
		const paddr_t m_paddr;
	};

}