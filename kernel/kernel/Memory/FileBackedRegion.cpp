#include <kernel/Lock/LockGuard.h>
#include <kernel/Memory/FileBackedRegion.h>
#include <kernel/Memory/Heap.h>

#include <sys/mman.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::UniqPtr<FileBackedRegion>> FileBackedRegion::create(BAN::RefPtr<Inode> inode, PageTable& page_table, off_t offset, size_t size, AddressRange address_range, Type type, PageTable::flags_t flags)
	{
		ASSERT(inode->mode().ifreg());

		if (offset < 0 || offset % PAGE_SIZE || size == 0)
			return BAN::Error::from_errno(EINVAL);
		if (size > (size_t)inode->size() || (size_t)offset > (size_t)inode->size() - size)
			return BAN::Error::from_errno(EOVERFLOW);

		auto* region_ptr = new FileBackedRegion(inode, page_table, offset, size, type, flags);
		if (region_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto region = BAN::UniqPtr<FileBackedRegion>::adopt(region_ptr);

		TRY(region->initialize(address_range));

		if (type == Type::SHARED)
		{
			LockGuard _(inode->m_mutex);
			if (inode->m_shared_region.valid())
				region->m_shared_data = inode->m_shared_region.lock();
			else
			{
				auto shared_data = TRY(BAN::RefPtr<SharedFileData>::create());
				TRY(shared_data->pages.resize(BAN::Math::div_round_up<size_t>(inode->size(), PAGE_SIZE)));
				shared_data->inode = inode;
				inode->m_shared_region = TRY(shared_data->get_weak_ptr());
				region->m_shared_data = BAN::move(shared_data);
			}
		}

		return region;
	}

	FileBackedRegion::FileBackedRegion(BAN::RefPtr<Inode> inode, PageTable& page_table, off_t offset, ssize_t size, Type type, PageTable::flags_t flags)
		: MemoryRegion(page_table, size, type, flags)
		, m_inode(inode)
		, m_offset(offset)
	{
	}

	FileBackedRegion::~FileBackedRegion()
	{
		if (m_vaddr == 0)
			return;

		if (m_type == Type::SHARED)
			return;

		size_t needed_pages = BAN::Math::div_round_up<size_t>(m_size, PAGE_SIZE);
		for (size_t i = 0; i < needed_pages; i++)
		{
			paddr_t paddr = m_page_table.physical_address_of(m_vaddr + i * PAGE_SIZE);
			if (paddr != 0)
				Heap::get().release_page(paddr);
		}
	}

	SharedFileData::~SharedFileData()
	{
		for (size_t i = 0; i < pages.size(); i++)
		{
			if (pages[i] == 0)
				continue;
			sync(i);
		}
	}

	void SharedFileData::sync(size_t page_index)
	{
		// FIXME: should this be locked?

		if (pages[page_index] == 0)
			return;

		PageTable::with_fast_page(pages[page_index], [&] {
			memcpy(page_buffer, PageTable::fast_page_as_ptr(), PAGE_SIZE);
		});

		if (auto ret = inode->write(page_index * PAGE_SIZE, BAN::ConstByteSpan::from(page_buffer)); ret.is_error())
			dwarnln("{}", ret.error());
	}

	BAN::ErrorOr<void> FileBackedRegion::msync(vaddr_t address, size_t size, int flags)
	{
		if (flags != MS_SYNC)
			return BAN::Error::from_errno(ENOTSUP);
		if (m_type != Type::SHARED)
			return {};

		vaddr_t first_page	= address & PAGE_ADDR_MASK;
		vaddr_t last_page	= BAN::Math::div_round_up<vaddr_t>(address + size, PAGE_SIZE) * PAGE_SIZE;

		for (vaddr_t page_addr = first_page; page_addr < last_page; page_addr += PAGE_SIZE)
			if (contains(page_addr))
				m_shared_data->sync((page_addr - m_vaddr) / PAGE_SIZE);

		return {};
	}

	BAN::ErrorOr<bool> FileBackedRegion::allocate_page_containing_impl(vaddr_t address)
	{
		ASSERT(contains(address));

		// Check if address is already mapped
		vaddr_t vaddr = address & PAGE_ADDR_MASK;
		if (m_page_table.physical_address_of(vaddr) != 0)
			return false;

		if (m_type == Type::PRIVATE)
		{
			// Map new physcial page to address
			paddr_t paddr = Heap::get().take_free_page();
			if (paddr == 0)
				return BAN::Error::from_errno(ENOMEM);

			// Temporarily force mapping to be writable so kernel can write to it
			m_page_table.map_page_at(paddr, vaddr, m_flags | PageTable::Flags::ReadWrite);

			size_t file_offset = m_offset + (vaddr - m_vaddr);
			size_t bytes = BAN::Math::min<size_t>(m_size - file_offset, PAGE_SIZE);

			ASSERT(&PageTable::current() == &m_page_table);
			auto read_ret = m_inode->read(file_offset, BAN::ByteSpan((uint8_t*)vaddr, bytes));

			if (read_ret.is_error())
			{
				Heap::get().release_page(paddr);
				m_page_table.unmap_page(vaddr);
				return read_ret.release_error();
			}

			if (read_ret.value() < bytes)
			{
				dwarnln("Only {}/{} bytes read", read_ret.value(), bytes);
				Heap::get().release_page(paddr);
				m_page_table.unmap_page(vaddr);
				return BAN::Error::from_errno(EIO);
			}

			// Disable writable if not wanted
			if (!(m_flags & PageTable::Flags::ReadWrite))
				m_page_table.map_page_at(paddr, vaddr, m_flags);
		}
		else if	(m_type == Type::SHARED)
		{
			LockGuard _(m_inode->m_mutex);
			ASSERT(m_inode->m_shared_region.valid());
			ASSERT(m_shared_data->pages.size() == BAN::Math::div_round_up<size_t>(m_inode->size(), PAGE_SIZE));

			auto& pages = m_shared_data->pages;
			size_t page_index = (vaddr - m_vaddr) / PAGE_SIZE;

			if (pages[page_index] == 0)
			{
				pages[page_index] = Heap::get().take_free_page();
				if (pages[page_index] == 0)
					return BAN::Error::from_errno(ENOMEM);

				size_t offset = vaddr - m_vaddr;
				size_t bytes = BAN::Math::min<size_t>(m_size - offset, PAGE_SIZE);

				TRY(m_inode->read(offset, BAN::ByteSpan(m_shared_data->page_buffer, bytes)));

				PageTable::with_fast_page(pages[page_index], [&] {
					memcpy(PageTable::fast_page_as_ptr(), m_shared_data->page_buffer, PAGE_SIZE);
				});
			}

			paddr_t paddr = pages[page_index];
			ASSERT(paddr);

			m_page_table.map_page_at(paddr, vaddr, m_flags);
		}
		else
		{
			ASSERT_NOT_REACHED();
		}

		return true;
	}

	BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> FileBackedRegion::clone(PageTable&)
	{
		ASSERT_NOT_REACHED();
	}

}
