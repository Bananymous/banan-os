#include <kernel/Memory/PageTable.h>
#include <kernel/Memory/PhysicalRange.h>

namespace Kernel
{

	PhysicalRange::PhysicalRange(paddr_t paddr, uint64_t size)
		: m_paddr(paddr)
		, m_page_count(size / PAGE_SIZE)
		, m_free_pages(m_page_count)
	{
		ASSERT(paddr % PAGE_SIZE == 0);
		ASSERT(size % PAGE_SIZE == 0);
		ASSERT(m_page_count);
		ASSERT(m_page_count <= PAGE_SIZE * 8);

		PageTable::with_per_cpu_fast_page(paddr, [](void* addr) {
			memset(addr, 0, PAGE_SIZE);
		});

		const paddr_t reserved = reserve_page();
		ASSERT(reserved == m_paddr);
	}

	paddr_t PhysicalRange::reserve_page()
	{
		ASSERT(free_pages() > 0);

		size_t matched;

		PageTable::with_per_cpu_fast_page(m_paddr, [&matched](void* addr) {
			uint64_t* qwords = static_cast<uint64_t*>(addr);
			for (size_t i = 0; i < PAGE_SIZE / sizeof(uint64_t); i++)
			{
				if (qwords[i] == UINT64_MAX)
					continue;
				const size_t bit = BAN::Math::ctz(~qwords[i]);
				qwords[i] |= 1ull << bit;
				matched = i * 64 + bit;
				return;
			}
			ASSERT_NOT_REACHED();
		});

		ASSERT(matched < m_page_count);

		m_free_pages--;

		return m_paddr + matched * PAGE_SIZE;
	}

	void PhysicalRange::release_page(paddr_t paddr)
	{
		ASSERT(paddr % PAGE_SIZE == 0);
		ASSERT(paddr != m_paddr);
		ASSERT(contains(paddr));

		const size_t index = (paddr - m_paddr) / PAGE_SIZE;
		const size_t qword = index / 64;
		const size_t bit   = index % 64;

		PageTable::with_per_cpu_fast_page(m_paddr, [qword, bit](void* addr) {
			uint64_t* qwords = static_cast<uint64_t*>(addr);
			ASSERT(qwords[qword] & (1ull << bit));
			qwords[qword] &= ~(1ull << bit);
		});

		m_free_pages++;
	}

	paddr_t PhysicalRange::reserve_contiguous_pages(size_t pages)
	{
		ASSERT(pages > 0);
		ASSERT(pages <= free_pages());

		size_t matched = SIZE_MAX;

		PageTable::with_per_cpu_fast_page(m_paddr, [this, &matched, pages](void* addr) {
			uint64_t* qwords = static_cast<uint64_t*>(addr);
			for (size_t i = 0; i < m_page_count - pages; i++)
			{
				bool is_usable = true;
				for (size_t j = i; j < i + pages && is_usable; j++)
					is_usable = !(qwords[j / 64] & (1ull << (j % 64)));
				if (!is_usable)
					continue;
				for (size_t j = i; j < i + pages; j++)
					qwords[j / 64] |= 1ull << (j % 64);
				matched = i;
				return;
			}
		});

		if (matched == SIZE_MAX)
			return 0;

		m_free_pages -= pages;

		return m_paddr + matched * PAGE_SIZE;
	}

	void PhysicalRange::release_contiguous_pages(paddr_t paddr, size_t pages)
	{
		ASSERT(pages > 0);
		ASSERT(paddr != m_paddr);
		ASSERT(contains(paddr));
		ASSERT(contains(paddr + (pages - 1) * PAGE_SIZE));

		const size_t index = (paddr - m_paddr) / PAGE_SIZE;

		PageTable::with_per_cpu_fast_page(m_paddr, [index, pages](void* addr) {
			uint64_t* qwords = static_cast<uint64_t*>(addr);
			for (size_t i = index; i < index + pages; i++)
			{
				ASSERT(qwords[i / 64] & (1ull << (i % 64)));
				qwords[i / 64] &= ~(1ull << (i % 64));
			}
		});

		m_free_pages += pages;
	}

}
