#pragma once

#include <BAN/LinkedList.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/MMU.h>

namespace Kernel
{

	class GeneralAllocator
	{
		BAN_NON_COPYABLE(GeneralAllocator);
		BAN_NON_MOVABLE(GeneralAllocator);

	public:
		GeneralAllocator(MMU&);
		~GeneralAllocator();

		vaddr_t allocate(size_t);
		bool deallocate(vaddr_t);

	private:
		struct Allocation
		{
			vaddr_t address { 0 };
			BAN::Vector<paddr_t> pages;
		};

	private:
		MMU& m_mmu;
		BAN::LinkedList<Allocation> m_allocations;
	};

}