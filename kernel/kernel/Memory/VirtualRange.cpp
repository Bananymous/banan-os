#include <BAN/ScopeGuard.h>
#include <kernel/LockGuard.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/VirtualRange.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> VirtualRange::create_to_vaddr(PageTable& page_table, vaddr_t vaddr, size_t size, PageTable::flags_t flags)
	{
		ASSERT(size % PAGE_SIZE == 0);
		ASSERT(vaddr % PAGE_SIZE == 0);
		ASSERT(vaddr > 0);

		VirtualRange* result_ptr = new VirtualRange(page_table);
		if (result_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto result = BAN::UniqPtr<VirtualRange>::adopt(result_ptr);

		result->m_kmalloc = false;
		result->m_vaddr = vaddr;
		result->m_size = size;
		result->m_flags = flags;
		TRY(result->m_physical_pages.reserve(size / PAGE_SIZE));

		ASSERT(page_table.reserve_range(vaddr, size));
		BAN::ScopeGuard unmapper([vaddr, size, &page_table] { page_table.unmap_range(vaddr, size); });

		TRY(result->m_physical_pages.reserve(size / PAGE_SIZE));
		for (size_t offset = 0; offset < size; offset += PAGE_SIZE)
		{
			paddr_t paddr = Heap::get().take_free_page();
			if (paddr == 0)
			{
				for (paddr_t release : result->m_physical_pages)
					Heap::get().release_page(release);
				return BAN::Error::from_errno(ENOMEM);
			}
			MUST(result->m_physical_pages.push_back(paddr));
			page_table.map_page_at(paddr, vaddr + offset, flags);
		}

		unmapper.disable();

		return result;
	}

	BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> VirtualRange::create_to_vaddr_range(PageTable& page_table, vaddr_t vaddr_start, vaddr_t vaddr_end, size_t size, PageTable::flags_t flags)
	{
		ASSERT(size % PAGE_SIZE == 0);
		ASSERT(vaddr_start > 0);

		// Align vaddr range to page boundaries
		if (size_t rem = vaddr_start % PAGE_SIZE)
			vaddr_start += PAGE_SIZE - rem;
		if (size_t rem = vaddr_end % PAGE_SIZE)
			vaddr_end -= rem;
		ASSERT(vaddr_start < vaddr_end);
		ASSERT(vaddr_end - vaddr_start + 1 >= size / PAGE_SIZE);

		VirtualRange* result_ptr = new VirtualRange(page_table);
		if (result_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto result = BAN::UniqPtr<VirtualRange>::adopt(result_ptr);

		result->m_kmalloc = false;
		result->m_size = size;
		result->m_flags = flags;
		TRY(result->m_physical_pages.reserve(size / PAGE_SIZE));

		vaddr_t vaddr = page_table.reserve_free_contiguous_pages(size / PAGE_SIZE, vaddr_start, vaddr_end);
		if (vaddr == 0)
			return BAN::Error::from_errno(ENOMEM);
		result->m_vaddr = vaddr;

		BAN::ScopeGuard unmapper([vaddr, size, &page_table] { page_table.unmap_range(vaddr, size); });
		if (vaddr + size > vaddr_end)
			return BAN::Error::from_errno(ENOMEM);

		result->m_vaddr = vaddr;

		TRY(result->m_physical_pages.reserve(size / PAGE_SIZE));
		for (size_t offset = 0; offset < size; offset += PAGE_SIZE)
		{
			paddr_t paddr = Heap::get().take_free_page();
			if (paddr == 0)
			{
				for (paddr_t release : result->m_physical_pages)
					Heap::get().release_page(release);
				return BAN::Error::from_errno(ENOMEM);
			}
			MUST(result->m_physical_pages.push_back(paddr));
			page_table.map_page_at(paddr, vaddr + offset, flags);
		}

		unmapper.disable();

		return result;
	}

	BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> VirtualRange::create_kmalloc(size_t size)
	{
		VirtualRange* result = new VirtualRange(PageTable::kernel());
		ASSERT(result);

		result->m_kmalloc = true;
		result->m_size = size;
		result->m_flags = PageTable::Flags::ReadWrite | PageTable::Flags::Present;
		result->m_vaddr = (vaddr_t)kmalloc(size);
		if (result->m_vaddr == 0)
		{
			delete result;
			return BAN::Error::from_errno(ENOMEM);
		}

		return BAN::UniqPtr<VirtualRange>::adopt(result);
	}

	VirtualRange::VirtualRange(PageTable& page_table)
		: m_page_table(page_table)
	{ }

	VirtualRange::~VirtualRange()
	{
		if (m_kmalloc)
		{
			kfree((void*)m_vaddr);
			return;
		}

		m_page_table.unmap_range(vaddr(), size());
		for (paddr_t page : m_physical_pages)
			Heap::get().release_page(page);
	}

	BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> VirtualRange::clone(PageTable& page_table)
	{
		auto result = TRY(create_to_vaddr(page_table, vaddr(), size(), flags()));

		m_page_table.lock();

		ASSERT(m_page_table.is_page_free(0));
		for (size_t i = 0; i < result->m_physical_pages.size(); i++)
		{
			m_page_table.map_page_at(result->m_physical_pages[i], 0, PageTable::Flags::ReadWrite | PageTable::Flags::Present);
			memcpy((void*)0, (void*)(vaddr() + i * PAGE_SIZE), PAGE_SIZE);
		}
		m_page_table.unmap_page(0);

		m_page_table.unlock();

		return result;
	}

	void VirtualRange::set_zero()
	{
		PageTable& page_table = PageTable::current();

		if (&page_table == &m_page_table)
		{
			memset((void*)vaddr(), 0, size());
			return;
		}

		page_table.lock();
		ASSERT(page_table.is_page_free(0));

		for (size_t i = 0; i < m_physical_pages.size(); i++)
		{
			page_table.map_page_at(m_physical_pages[i], 0, PageTable::Flags::ReadWrite | PageTable::Flags::Present);
			memset((void*)0, 0, PAGE_SIZE);
		}
		page_table.unmap_page(0);

		page_table.unlock();
	}

	void VirtualRange::copy_from(size_t offset, const uint8_t* buffer, size_t bytes)
	{
		if (bytes == 0)
			return;

		// NOTE: Handling overflow
		ASSERT(offset <= size());
		ASSERT(bytes <= size());
		ASSERT(offset + bytes <= size());

		PageTable& page_table = PageTable::current();

		if (&page_table == &m_page_table)
		{
			memcpy((void*)(vaddr() + offset), buffer, bytes);
			return;
		}

		page_table.lock();
		ASSERT(page_table.is_page_free(0));

		size_t off = offset % PAGE_SIZE;
		size_t i = offset / PAGE_SIZE;

		// NOTE: we map the first page separately since it needs extra calculations
		page_table.map_page_at(m_physical_pages[i], 0, PageTable::Flags::ReadWrite | PageTable::Flags::Present);

		memcpy((void*)off, buffer, PAGE_SIZE - off);

		buffer += PAGE_SIZE - off;
		bytes  -= PAGE_SIZE - off;
		i++;

		while (bytes > 0)
		{
			size_t len = BAN::Math::min<size_t>(PAGE_SIZE, bytes);

			page_table.map_page_at(m_physical_pages[i], 0, PageTable::Flags::ReadWrite | PageTable::Flags::Present);

			memcpy((void*)0, buffer, len);

			buffer += len;
			bytes  -= len;
			i++;
		}
		page_table.unmap_page(0);

		page_table.unlock();
	}

}