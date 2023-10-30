#include <kernel/CriticalScope.h>
#include <kernel/LockGuard.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/VirtualRange.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> VirtualRange::create_to_vaddr(PageTable& page_table, vaddr_t vaddr, size_t size, PageTable::flags_t flags, bool preallocate_pages)
	{
		ASSERT(size % PAGE_SIZE == 0);
		ASSERT(vaddr % PAGE_SIZE == 0);
		ASSERT(vaddr > 0);

		VirtualRange* result_ptr = new VirtualRange(page_table, preallocate_pages, false);
		if (result_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);

		auto result = BAN::UniqPtr<VirtualRange>::adopt(result_ptr);
		result->m_vaddr = vaddr;
		result->m_size = size;
		result->m_flags = flags;

		ASSERT(page_table.reserve_range(vaddr, size));

		if (!preallocate_pages)
			return result;

		size_t needed_pages = size / PAGE_SIZE;
		for (size_t i = 0; i < needed_pages; i++)
		{
			paddr_t paddr = Heap::get().take_free_page();
			if (paddr == 0)
			{
				for (size_t j = 0; j < i; j++)
					Heap::get().release_page(page_table.physical_address_of(vaddr + j * PAGE_SIZE));
				page_table.unmap_range(vaddr, size);
				result->m_vaddr = 0;
				return BAN::Error::from_errno(ENOMEM);
			}
			page_table.map_page_at(paddr, vaddr + i * PAGE_SIZE, flags);
		}

		result->set_zero();

		return result;
	}

	BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> VirtualRange::create_to_vaddr_range(PageTable& page_table, vaddr_t vaddr_start, vaddr_t vaddr_end, size_t size, PageTable::flags_t flags, bool preallocate_pages)
	{
		ASSERT(size % PAGE_SIZE == 0);
		ASSERT(vaddr_start > 0);
		ASSERT(vaddr_start + size <= vaddr_end);

		// Align vaddr range to page boundaries
		if (size_t rem = vaddr_start % PAGE_SIZE)
			vaddr_start += PAGE_SIZE - rem;
		if (size_t rem = vaddr_end % PAGE_SIZE)
			vaddr_end -= rem;
		ASSERT(vaddr_start < vaddr_end);
		ASSERT(vaddr_end - vaddr_start + 1 >= size / PAGE_SIZE);

		vaddr_t vaddr = page_table.reserve_free_contiguous_pages(size / PAGE_SIZE, vaddr_start, vaddr_end);
		if (vaddr == 0)
		{
			dprintln("no free {} byte area", size);
			return BAN::Error::from_errno(ENOMEM);
		}
		ASSERT(vaddr + size <= vaddr_end);

		LockGuard _(page_table);
		page_table.unmap_range(vaddr, size); // We have to unmap here to allow reservation in create_to_vaddr()
		return create_to_vaddr(page_table, vaddr, size, flags, preallocate_pages);
	}

	BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> VirtualRange::create_kmalloc(size_t size)
	{
		VirtualRange* result = new VirtualRange(PageTable::kernel(), false, true);
		ASSERT(result);

		result->m_size = size;
		result->m_flags = PageTable::Flags::ReadWrite | PageTable::Flags::Present;
		result->m_vaddr = (vaddr_t)kmalloc(size);
		if (result->m_vaddr == 0)
		{
			delete result;
			return BAN::Error::from_errno(ENOMEM);
		}

		result->set_zero();

		return BAN::UniqPtr<VirtualRange>::adopt(result);
	}

	VirtualRange::VirtualRange(PageTable& page_table, bool preallocated, bool kmalloc)
		: m_page_table(page_table)
		, m_preallocated(preallocated)
		, m_kmalloc(kmalloc)
	{ }

	VirtualRange::~VirtualRange()
	{
		if (m_vaddr == 0)
			return;

		if (m_kmalloc)
			kfree((void*)m_vaddr);
		else
		{
			for (size_t offset = 0; offset < size(); offset += PAGE_SIZE)
			{
				paddr_t paddr = m_page_table.physical_address_of(vaddr() + offset);
				if (paddr)
					Heap::get().release_page(paddr);
			}
			m_page_table.unmap_range(vaddr(), size());
		}
	}

	BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> VirtualRange::clone(PageTable& page_table)
	{
		ASSERT(&PageTable::current() == &m_page_table);
		ASSERT(&m_page_table != &page_table);

		auto result = TRY(create_to_vaddr(page_table, vaddr(), size(), flags(), m_preallocated));

		LockGuard _(m_page_table);
		for (size_t offset = 0; offset < size(); offset += PAGE_SIZE)
		{
			if (!m_preallocated && m_page_table.physical_address_of(vaddr() + offset))
			{
				paddr_t paddr = Heap::get().take_free_page();
				if (paddr == 0)
					return BAN::Error::from_errno(ENOMEM);
				result->m_page_table.map_page_at(paddr, vaddr() + offset, m_flags);					
			}

			CriticalScope _;
			PageTable::map_fast_page(result->m_page_table.physical_address_of(vaddr() + offset));
			memcpy(PageTable::fast_page_as_ptr(), (void*)(vaddr() + offset), PAGE_SIZE);
			PageTable::unmap_fast_page();
		}

		return result;
	}

	BAN::ErrorOr<void> VirtualRange::allocate_page_for_demand_paging(vaddr_t address)
	{
		ASSERT(!m_kmalloc);
		ASSERT(!m_preallocated);
		ASSERT(contains(address));
		ASSERT(&PageTable::current() == &m_page_table);

		vaddr_t vaddr = address & PAGE_ADDR_MASK;
		ASSERT(m_page_table.physical_address_of(vaddr) == 0);

		paddr_t paddr = Heap::get().take_free_page();
		if (paddr == 0)
			return BAN::Error::from_errno(ENOMEM);

		m_page_table.map_page_at(paddr, vaddr, m_flags);
		memset((void*)vaddr, 0x00, PAGE_SIZE);

		return {};
	}

	void VirtualRange::set_zero()
	{
		PageTable& page_table = PageTable::current();

		if (m_kmalloc || &page_table == &m_page_table)
		{
			memset((void*)vaddr(), 0, size());
			return;
		}

		for (size_t offset = 0; offset < size(); offset += PAGE_SIZE)
		{
			CriticalScope _;
			PageTable::map_fast_page(m_page_table.physical_address_of(vaddr() + offset));
			memset(PageTable::fast_page_as_ptr(), 0x00, PAGE_SIZE);
			PageTable::unmap_fast_page();
		}
	}

	void VirtualRange::copy_from(size_t offset, const uint8_t* buffer, size_t bytes)
	{
		if (bytes == 0)
			return;

		// Verify no overflow
		ASSERT_LE(bytes, size());
		ASSERT_LE(offset, size());
		ASSERT_LE(offset, size() - bytes);

		if (m_kmalloc || &PageTable::current() == &m_page_table)
		{
			memcpy((void*)(vaddr() + offset), buffer, bytes);
			return;
		}

		size_t page_offset = offset % PAGE_SIZE;
		size_t page_index = offset / PAGE_SIZE;

		while (bytes > 0)
		{
			{
				CriticalScope _;
				PageTable::map_fast_page(m_page_table.physical_address_of(vaddr() + page_index * PAGE_SIZE));
				memcpy(PageTable::fast_page_as_ptr(page_offset), buffer, PAGE_SIZE - page_offset);
				PageTable::unmap_fast_page();
			}

			buffer += PAGE_SIZE - page_offset;
			bytes  -= PAGE_SIZE - page_offset;
			page_offset = 0;
			page_index++;
		}
	}

}