#include <kernel/Memory/Heap.h>
#include <kernel/Memory/VirtualRange.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> VirtualRange::create(PageTable& page_table, vaddr_t vaddr, size_t size, uint8_t flags)
	{
		ASSERT(size % PAGE_SIZE == 0);
		ASSERT(vaddr % PAGE_SIZE == 0);

		VirtualRange* result = new VirtualRange(page_table);
		ASSERT(result);

		result->m_kmalloc = false;
		result->m_size = size;
		result->m_flags = flags;
		MUST(result->m_physical_pages.reserve(size / PAGE_SIZE));

		page_table.lock();

		if (vaddr == 0)
		{
			vaddr = page_table.get_free_contiguous_pages(size / PAGE_SIZE, 0x300000);
			ASSERT(vaddr);
		}

		result->m_vaddr = vaddr;

		ASSERT(page_table.is_range_free(vaddr, size));
		for (size_t offset = 0; offset < size; offset += PAGE_SIZE)
		{
			paddr_t paddr = Heap::get().take_free_page();
			ASSERT(paddr);
			MUST(result->m_physical_pages.push_back(paddr));
			page_table.map_page_at(paddr, vaddr + offset, flags);
		}
		page_table.unlock();

		return BAN::UniqPtr<VirtualRange>::adopt(result);
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
			return BAN::UniqPtr<VirtualRange>();
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
		auto result = TRY(create(page_table, vaddr(), size(), flags()));

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