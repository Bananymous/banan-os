#include <kernel/LockGuard.h>
#include <kernel/Memory/FileBackedRegion.h>
#include <kernel/Memory/Heap.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::UniqPtr<FileBackedRegion>> FileBackedRegion::create(BAN::RefPtr<Inode> inode, PageTable& page_table, off_t offset, size_t size, AddressRange address_range, Type type, PageTable::flags_t flags)
	{
		ASSERT(inode->mode().ifreg());

		if (type != Type::PRIVATE)
			return BAN::Error::from_errno(ENOTSUP);

		if (offset < 0 || offset % PAGE_SIZE || size == 0)
			return BAN::Error::from_errno(EINVAL);
		if (size > (size_t)inode->size() || (size_t)offset > (size_t)inode->size() - size)
			return BAN::Error::from_errno(EOVERFLOW);

		auto* region_ptr = new FileBackedRegion(inode, page_table, offset, size, type, flags);
		if (region_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto region = BAN::UniqPtr<FileBackedRegion>::adopt(region_ptr);

		TRY(region->initialize(address_range));

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
		
		ASSERT(m_type == Type::PRIVATE);

		size_t needed_pages = BAN::Math::div_round_up<size_t>(m_size, PAGE_SIZE);
		for (size_t i = 0; i < needed_pages; i++)
		{
			paddr_t paddr = m_page_table.physical_address_of(m_vaddr + i * PAGE_SIZE);
			if (paddr != 0)
				Heap::get().release_page(paddr);
		}
	}

	BAN::ErrorOr<bool> FileBackedRegion::allocate_page_containing(vaddr_t address)
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

		size_t file_offset = m_offset + (vaddr - m_vaddr);
		size_t bytes = BAN::Math::min<size_t>(m_size - file_offset, PAGE_SIZE);

		BAN::ErrorOr<size_t> read_ret = 0;

		// Zero out the new page
		if (&PageTable::current() == &m_page_table)
			read_ret = m_inode->read(file_offset, (void*)vaddr, bytes);
		else
		{
			LockGuard _(PageTable::current());
			ASSERT(PageTable::current().is_page_free(0));

			PageTable::current().map_page_at(paddr, 0, PageTable::Flags::ReadWrite | PageTable::Flags::Present);
			read_ret = m_inode->read(file_offset, (void*)0, bytes);
			memset((void*)0, 0x00, PAGE_SIZE);
			PageTable::current().unmap_page(0);
		}

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

		return true;
	}

	BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> FileBackedRegion::clone(PageTable& new_page_table)
	{
		ASSERT_NOT_REACHED();
	}

}
