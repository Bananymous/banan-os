#pragma once

#include <BAN/NoCopyMove.h>
#include <BAN/Vector.h>

#include <kernel/Memory/PhysicalRange.h>
#include <kernel/SpinLock.h>

namespace Kernel
{
	
	class Heap
	{
		BAN_NON_COPYABLE(Heap);
		BAN_NON_MOVABLE(Heap);

	public:
		static void initialize();
		static Heap& get();

		paddr_t take_free_page();
		void release_page(paddr_t);

	private:
		Heap() = default;
		void initialize_impl();

	private:
		BAN::Vector<PhysicalRange>	m_physical_ranges;
		SpinLock					m_lock;
	};

}
