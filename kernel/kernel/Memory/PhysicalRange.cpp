#include <BAN/Assert.h>
#include <BAN/Math.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/Memory/PhysicalRange.h>

namespace Kernel
{

	using ull = unsigned long long;

	static constexpr ull ull_bits = sizeof(ull) * 8;

	PhysicalRange::PhysicalRange(paddr_t paddr, size_t size)
		: m_paddr(paddr)
		, m_size(size)
		, m_bitmap_pages(BAN::Math::div_round_up<size_t>(size / PAGE_SIZE, 8))
		, m_data_pages((size / PAGE_SIZE) - m_bitmap_pages)
		, m_free_pages(m_data_pages)
	{
		ASSERT(paddr % PAGE_SIZE == 0);
		ASSERT(size % PAGE_SIZE == 0);
		ASSERT(m_bitmap_pages < size / PAGE_SIZE);

		m_vaddr = PageTable::kernel().reserve_free_contiguous_pages(m_bitmap_pages, KERNEL_OFFSET);
		ASSERT(m_vaddr);
		PageTable::kernel().map_range_at(m_paddr, m_vaddr, size, PageTable::Flags::ReadWrite | PageTable::Flags::Present);

		memset((void*)m_vaddr, 0x00, m_bitmap_pages * PAGE_SIZE);
		memset((void*)m_vaddr, 0xFF, m_data_pages / 8);
		for (int i = 0; i < m_data_pages % 8; i++)
			((uint8_t*)m_vaddr)[m_data_pages / 8] |= 1 << i;

		dprintln("physical range needs {} pages for bitmap", m_bitmap_pages);
	}

	paddr_t PhysicalRange::paddr_for_bit(ull bit) const
	{
		return m_paddr + (m_bitmap_pages + bit) * PAGE_SIZE;
	}

	ull PhysicalRange::bit_for_paddr(paddr_t paddr) const
	{
		return (paddr - m_paddr) / PAGE_SIZE - m_bitmap_pages;
	}

	paddr_t PhysicalRange::reserve_page()
	{
		ASSERT(free_pages() > 0);

		ull ull_count = BAN::Math::div_round_up<ull>(m_data_pages, ull_bits);

		for (ull i = 0; i < ull_count; i++)
		{
			if (ull_bitmap_ptr()[i] == 0)
				continue;

			int lsb = __builtin_ctzll(ull_bitmap_ptr()[i]);

			ull_bitmap_ptr()[i] &= ~(1ull << lsb);
			m_free_pages--;
			return paddr_for_bit(i * ull_bits + lsb);
		}

		ASSERT_NOT_REACHED();
	}

	void PhysicalRange::release_page(paddr_t paddr)
	{
		ASSERT(paddr % PAGE_SIZE == 0);
		ASSERT(paddr - m_paddr <= m_size);

		ull full_bit = bit_for_paddr(paddr);
		ull off = full_bit / ull_bits;
		ull bit = full_bit % ull_bits;
		ull mask = 1ull << bit;

		ASSERT(!(ull_bitmap_ptr()[off] & mask));
		ull_bitmap_ptr()[off] |= mask;

		m_free_pages++;
	}

}
