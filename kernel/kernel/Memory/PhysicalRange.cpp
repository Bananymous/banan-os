#include <BAN/Assert.h>
#include <BAN/Math.h>
#include <BAN/Optional.h>

#include <kernel/Memory/PageTable.h>
#include <kernel/Memory/PhysicalRange.h>

namespace Kernel
{

	static constexpr size_t bits_per_page = PAGE_SIZE * 8;

	PhysicalRange::PhysicalRange(paddr_t paddr, size_t size)
		: m_paddr(paddr)
		, m_page_count(size / PAGE_SIZE)
		, m_free_pages(m_page_count)
	{
		ASSERT(paddr % PAGE_SIZE == 0);
		ASSERT(size % PAGE_SIZE == 0);

		const size_t bitmap_page_count = BAN::Math::div_round_up<size_t>(m_page_count, bits_per_page);
		for (size_t i = 0; i < bitmap_page_count; i++)
		{
			PageTable::with_fast_page(paddr + i * PAGE_SIZE, [] {
				memset(PageTable::fast_page_as_ptr(), 0, PAGE_SIZE);
			});
		}

		ASSERT(reserve_contiguous_pages(bitmap_page_count) == m_paddr);
	}

	paddr_t PhysicalRange::reserve_page()
	{
		ASSERT(free_pages() > 0);

		const size_t bitmap_page_count = BAN::Math::div_round_up<size_t>(m_page_count, bits_per_page);

		for (size_t i = 0; i < bitmap_page_count; i++)
		{
			BAN::Optional<size_t> page_matched_bit;

			const paddr_t current_paddr = m_paddr + i * PAGE_SIZE;
			PageTable::with_fast_page(current_paddr, [&page_matched_bit] {
				for (size_t j = 0; j < PAGE_SIZE / sizeof(size_t); j++)
				{
					static_assert(sizeof(size_t) == sizeof(long));
					const size_t current = PageTable::fast_page_as_sized<volatile size_t>(j);
					if (current == BAN::numeric_limits<size_t>::max())
						continue;
					const int ctz = __builtin_ctzl(~current);
					PageTable::fast_page_as_sized<volatile size_t>(j) = current | (static_cast<size_t>(1) << ctz);
					page_matched_bit = j * sizeof(size_t) * 8 + ctz;
					return;
				}
			});

			if (page_matched_bit.has_value())
			{
				m_free_pages--;

				const size_t matched_bit = (i * bits_per_page) + page_matched_bit.value();
				ASSERT(matched_bit < m_page_count);
				return m_paddr + matched_bit * PAGE_SIZE;
			}
		}

		ASSERT_NOT_REACHED();
	}

	void PhysicalRange::release_page(paddr_t paddr)
	{
		ASSERT(paddr % PAGE_SIZE == 0);
		ASSERT(paddr >= m_paddr);
		ASSERT(paddr <  m_paddr + m_page_count * PAGE_SIZE);

		const size_t paddr_index = (paddr - m_paddr) / PAGE_SIZE;

		PageTable::with_fast_page(m_paddr + paddr_index / bits_per_page * PAGE_SIZE, [paddr_index] {
			const size_t bitmap_bit = paddr_index % bits_per_page;
			const size_t byte       = bitmap_bit / 8;
			const size_t bit        = bitmap_bit % 8;

			volatile uint8_t& bitmap_byte = PageTable::fast_page_as_sized<volatile uint8_t>(byte);
			ASSERT(bitmap_byte & (1u << bit));

			bitmap_byte = bitmap_byte & ~(1u << bit);
		});

		m_free_pages++;
	}

	paddr_t PhysicalRange::reserve_contiguous_pages(size_t pages)
	{
		ASSERT(pages > 0);
		ASSERT(pages <= free_pages());

		const auto bitmap_is_set =
			[this](size_t buffer_bit) -> bool
			{
				const size_t page_index = buffer_bit / bits_per_page;
				const size_t bit_index  = buffer_bit % bits_per_page;
				const size_t byte = bit_index / 8;
				const size_t bit  = bit_index % 8;

				uint8_t current;
				PageTable::with_fast_page(m_paddr + page_index * PAGE_SIZE, [&current, byte] {
					current = PageTable::fast_page_as_sized<volatile uint8_t>(byte);
				});

				return current & (1u << bit);
			};

		const auto bitmap_set_bit =
			[this](size_t buffer_bit) -> void
			{
				const size_t page_index = buffer_bit / bits_per_page;
				const size_t bit_index  = buffer_bit % bits_per_page;
				const size_t byte = bit_index / 8;
				const size_t bit  = bit_index % 8;
				PageTable::with_fast_page(m_paddr + page_index * PAGE_SIZE, [byte, bit] {
					volatile uint8_t& current = PageTable::fast_page_as_sized<volatile uint8_t>(byte);
					current = current | (1u << bit);
				});
			};

		// FIXME: optimize this :)
		for (size_t i = 0; i <= m_page_count - pages; i++)
		{
			bool all_unset = true;
			for (size_t j = 0; j < pages && all_unset; j++)
				if (bitmap_is_set(i + j))
					all_unset = false;
			if (!all_unset)
				continue;
			for (size_t j = 0; j < pages; j++)
				bitmap_set_bit(i + j);

			m_free_pages -= pages;
			return m_paddr + i * PAGE_SIZE;
		}

		return 0;
	}

	void PhysicalRange::release_contiguous_pages(paddr_t paddr, size_t pages)
	{
		ASSERT(pages > 0);
		// FIXME: optimize this :)
		for (size_t i = 0; i < pages; i++)
			release_page(paddr + i * PAGE_SIZE);
	}

}
