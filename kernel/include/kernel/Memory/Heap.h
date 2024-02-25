#pragma once

#include <BAN/NoCopyMove.h>
#include <BAN/Vector.h>

#include <kernel/Lock/SpinLock.h>
#include <kernel/Memory/PhysicalRange.h>

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

		paddr_t take_free_contiguous_pages(size_t pages);
		void release_contiguous_pages(paddr_t paddr, size_t pages);

		size_t used_pages() const;
		size_t free_pages() const;

	private:
		Heap() = default;
		void initialize_impl();

	private:
		BAN::Vector<PhysicalRange>	m_physical_ranges;
		mutable SpinLock			m_lock;
	};

}
