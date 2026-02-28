#include <kernel/Memory/ByteRingBuffer.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/PageTable.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::UniqPtr<ByteRingBuffer>> ByteRingBuffer::create(size_t size)
	{
		ASSERT(size % PAGE_SIZE == 0);

		const size_t page_count = size / PAGE_SIZE;

		auto* buffer_ptr = new ByteRingBuffer(size);
		if (buffer_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto buffer = BAN::UniqPtr<ByteRingBuffer>::adopt(buffer_ptr);

		buffer->m_vaddr = PageTable::kernel().reserve_free_contiguous_pages(page_count * 2, KERNEL_OFFSET);
		if (buffer->m_vaddr == 0)
			return BAN::Error::from_errno(ENOMEM);

		for (size_t i = 0; i < page_count; i++)
		{
			const paddr_t paddr = Heap::get().take_free_page();
			if (paddr == 0)
				return BAN::Error::from_errno(ENOMEM);
			PageTable::kernel().map_page_at(paddr, buffer->m_vaddr        + i * PAGE_SIZE, PageTable::ReadWrite | PageTable::Present);
			PageTable::kernel().map_page_at(paddr, buffer->m_vaddr + size + i * PAGE_SIZE, PageTable::ReadWrite | PageTable::Present);
		}

		return buffer;
	}

	ByteRingBuffer::~ByteRingBuffer()
	{
		if (m_vaddr == 0)
			return;
		for (size_t i = 0; i < m_capacity / PAGE_SIZE; i++)
		{
			const paddr_t paddr = PageTable::kernel().physical_address_of(m_vaddr + i * PAGE_SIZE);
			if (paddr == 0)
				break;
			Heap::get().release_page(paddr);
		}
		PageTable::kernel().unmap_range(m_vaddr, m_capacity * 2);
	}

}
