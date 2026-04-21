#include <BAN/ScopeGuard.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/VirtualRange.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> VirtualRange::create_to_vaddr_range(PageTable& page_table, AddressRange address_range, size_t size, PageTable::flags_t flags, bool add_guard_pages)
	{
		if (add_guard_pages)
			size += 2 * PAGE_SIZE;

		ASSERT(size % PAGE_SIZE == 0);

		// Align vaddr range to page boundaries
		if (const size_t rem = address_range.start % PAGE_SIZE)
			address_range.start += PAGE_SIZE - rem;
		if (const size_t rem = address_range.end % PAGE_SIZE)
			address_range.end -= rem;

		const vaddr_t vaddr = page_table.reserve_free_contiguous_pages(size / PAGE_SIZE, address_range.start, address_range.end);
		if (vaddr == 0)
			return BAN::Error::from_errno(ENOMEM);

		BAN::ScopeGuard vaddr_cleaner([&page_table, vaddr, size] {
			page_table.unmap_range(vaddr, size);
		});

		auto result = TRY(BAN::UniqPtr<VirtualRange>::create(
			page_table,
			add_guard_pages,
			vaddr,
			size,
			flags)
		);
		TRY(result->initialize());

		vaddr_cleaner.disable();

		return result;
	}

	VirtualRange::VirtualRange(PageTable& page_table, bool has_guard_pages, vaddr_t vaddr, size_t size, PageTable::flags_t flags)
		: m_page_table(page_table)
		, m_has_guard_pages(has_guard_pages)
		, m_vaddr(vaddr)
		, m_size(size)
		, m_flags(flags)
	{ }

	VirtualRange::~VirtualRange()
	{
		ASSERT(m_vaddr);
		for (size_t off = 0; off < size(); off += PAGE_SIZE)
			if (const auto paddr = m_page_table.physical_address_of(vaddr() + off))
				Heap::get().release_page(paddr);
		m_page_table.unmap_range(m_vaddr, m_size);
	}

	BAN::ErrorOr<void> VirtualRange::initialize()
	{
		const size_t page_count = size() / PAGE_SIZE;
		for (size_t i = 0; i < page_count; i++)
		{
			const auto paddr = Heap::get().take_free_page();
			if (paddr == 0)
				return BAN::Error::from_errno(ENOMEM);
			PageTable::with_fast_page(paddr, [] {
				memset(PageTable::fast_page_as_ptr(), 0, PAGE_SIZE);
			});
			m_page_table.map_page_at(paddr, vaddr() + i * PAGE_SIZE, m_flags);
		}
		return {};
	}

}
