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
		, m_bitmap_pages(BAN::Math::div_round_up<size_t>(size / PAGE_SIZE, PAGE_SIZE * 8))
		, m_data_pages((size / PAGE_SIZE) - m_bitmap_pages)
		, m_free_pages(m_data_pages)
	{
		ASSERT(paddr % PAGE_SIZE == 0);
		ASSERT(size % PAGE_SIZE == 0);
		ASSERT(m_bitmap_pages < size / PAGE_SIZE);

		m_vaddr = PageTable::kernel().reserve_free_contiguous_pages(m_bitmap_pages, KERNEL_OFFSET);
		ASSERT(m_vaddr);
		PageTable::kernel().map_range_at(m_paddr, m_vaddr, m_bitmap_pages * PAGE_SIZE, PageTable::Flags::ReadWrite | PageTable::Flags::Present);

		memset((void*)m_vaddr, 0x00, m_bitmap_pages * PAGE_SIZE);

		for (ull i = 0; i < m_data_pages / ull_bits; i++)
			ull_bitmap_ptr()[i] = ~0ull;

		if (m_data_pages % ull_bits)
		{
			ull off = m_data_pages / ull_bits;
			ull bits = m_data_pages % ull_bits;
			ull_bitmap_ptr()[off] = ~(~0ull << bits);
		}
	}

	paddr_t PhysicalRange::paddr_for_bit(ull bit) const
	{
		return m_paddr + (m_bitmap_pages + bit) * PAGE_SIZE;
	}

	ull PhysicalRange::bit_for_paddr(paddr_t paddr) const
	{
		return (paddr - m_paddr) / PAGE_SIZE - m_bitmap_pages;
	}

	ull PhysicalRange::contiguous_bits_set(ull start, ull count) const
	{
		for (ull i = 0; i < count; i++)
		{
			ull off = (start + i) / ull_bits;
			ull bit = (start + i) % ull_bits;
			if (!(ull_bitmap_ptr()[off] & (1ull << bit)))
				return i;
		}
		return count;
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

	paddr_t PhysicalRange::reserve_contiguous_pages(size_t pages)
	{
		ASSERT(pages > 0);
		ASSERT(free_pages() > 0);

		if (pages == 1)
			return reserve_page();

		ull ull_count = BAN::Math::div_round_up<ull>(m_data_pages, ull_bits);

		// NOTE: This feels kinda slow, but I don't want to be
		//       doing premature optimization. This will be only
		//       used when creating DMA regions.

		for (ull i = 0; i < ull_count; i++)
		{
			if (ull_bitmap_ptr()[i] == 0)
				continue;

			for (ull bit = 0; bit < ull_bits;)
			{
				ull start = i * ull_bits + bit;
				ull set_cnt = contiguous_bits_set(start, pages);
				if (set_cnt == pages)
				{
					for (ull j = 0; j < pages; j++)
						ull_bitmap_ptr()[(start + j) / ull_bits] &= ~(1ull << ((start + j) % ull_bits));
					m_free_pages -= pages;
					return paddr_for_bit(start);
				}
				bit += set_cnt + 1;
			}
		}

		ASSERT_NOT_REACHED();
	}

	void PhysicalRange::release_contiguous_pages(paddr_t paddr, size_t pages)
	{
		ASSERT(paddr % PAGE_SIZE == 0);
		ASSERT(paddr - m_paddr <= m_size);
		ASSERT(pages > 0);

		ull start_bit = bit_for_paddr(paddr);
		for (size_t i = 0; i < pages; i++)
		{
			ull off = (start_bit + i) / ull_bits;
			ull bit = (start_bit + i) % ull_bits;
			ull mask = 1ull << bit;

			ASSERT(!(ull_bitmap_ptr()[off] & mask));
			ull_bitmap_ptr()[off] |= mask;
		}

		m_free_pages += pages;
	}

}
