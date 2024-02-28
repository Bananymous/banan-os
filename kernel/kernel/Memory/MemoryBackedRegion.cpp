#include <kernel/CriticalScope.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/MemoryBackedRegion.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::UniqPtr<MemoryBackedRegion>> MemoryBackedRegion::create(PageTable& page_table, size_t size, AddressRange address_range, Type type, PageTable::flags_t flags)
	{
		if (type != Type::PRIVATE)
			return BAN::Error::from_errno(ENOTSUP);

		auto* region_ptr = new MemoryBackedRegion(page_table, size, type, flags);
		if (region_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto region = BAN::UniqPtr<MemoryBackedRegion>::adopt(region_ptr);

		TRY(region->initialize(address_range));

		return region;
	}

	MemoryBackedRegion::MemoryBackedRegion(PageTable& page_table, size_t size, Type type, PageTable::flags_t flags)
		: MemoryRegion(page_table, size, type, flags)
	{
	}

	MemoryBackedRegion::~MemoryBackedRegion()
	{
		ASSERT(m_type == Type::PRIVATE);

		size_t needed_pages = BAN::Math::div_round_up<size_t>(m_size, PAGE_SIZE);
		for (size_t i = 0; i < needed_pages; i++)
		{
			paddr_t paddr = m_page_table.physical_address_of(m_vaddr + i * PAGE_SIZE);
			if (paddr != 0)
				Heap::get().release_page(paddr);
		}
	}

	BAN::ErrorOr<bool> MemoryBackedRegion::allocate_page_containing_impl(vaddr_t address)
	{
		ASSERT(m_type == Type::PRIVATE);

		ASSERT(contains(address));

		// Check if address is already mapped
		vaddr_t vaddr = address & PAGE_ADDR_MASK;
		if (m_page_table.physical_address_of(vaddr) != 0)
			return false;

		// Map new physcial page to address
		paddr_t paddr = Heap::get().take_free_page();
		if (paddr == 0)
			return BAN::Error::from_errno(ENOMEM);
		m_page_table.map_page_at(paddr, vaddr, m_flags);

		// Zero out the new page
		PageTable::with_fast_page(paddr, [&] {
			memset(PageTable::fast_page_as_ptr(), 0x00, PAGE_SIZE);
		});

		return true;
	}

	BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> MemoryBackedRegion::clone(PageTable& new_page_table)
	{
		ASSERT(&PageTable::current() == &m_page_table);

		auto result = TRY(MemoryBackedRegion::create(new_page_table, m_size, { .start = m_vaddr, .end = m_vaddr + m_size }, m_type, m_flags));

		for (size_t offset = 0; offset < m_size; offset += PAGE_SIZE)
		{
			paddr_t paddr = m_page_table.physical_address_of(m_vaddr + offset);
			if (paddr == 0)
				continue;
			TRY(result->copy_data_to_region(offset, (const uint8_t*)(m_vaddr + offset), PAGE_SIZE));
		}

		return BAN::UniqPtr<MemoryRegion>(BAN::move(result));
	}

	BAN::ErrorOr<void> MemoryBackedRegion::copy_data_to_region(size_t offset_into_region, const uint8_t* buffer, size_t buffer_size)
	{
		ASSERT(offset_into_region + buffer_size <= m_size);

		size_t written = 0;
		while (written < buffer_size)
		{
			vaddr_t write_vaddr = m_vaddr + offset_into_region + written;
			vaddr_t page_offset = write_vaddr % PAGE_SIZE;
			size_t bytes = BAN::Math::min<size_t>(buffer_size - written, PAGE_SIZE - page_offset);

			TRY(allocate_page_containing(write_vaddr));

			PageTable::with_fast_page(m_page_table.physical_address_of(write_vaddr & PAGE_ADDR_MASK), [&] {
				memcpy(PageTable::fast_page_as_ptr(page_offset), (void*)(buffer + written), bytes);
			});

			written += bytes;
		}

		return {};
	}

}
