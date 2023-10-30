#include <kernel/CriticalScope.h>
#include <kernel/LockGuard.h>
#include <kernel/Memory/FileBackedRegion.h>
#include <kernel/Memory/Heap.h>

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
			LockGuard _(inode->m_lock);
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
			
			{
				CriticalScope _;
				PageTable::map_fast_page(pages[i]);
				memcpy(page_buffer, PageTable::fast_page_as_ptr(), PAGE_SIZE);
				PageTable::unmap_fast_page();
			}

			if (auto ret = inode->write(i * PAGE_SIZE, BAN::ConstByteSpan::from(page_buffer)); ret.is_error())
				dwarnln("{}", ret.error());
		}
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
			m_page_table.map_page_at(paddr, vaddr, m_flags);

			size_t file_offset = m_offset + (vaddr - m_vaddr);
			size_t bytes = BAN::Math::min<size_t>(m_size - file_offset, PAGE_SIZE);

			ASSERT_EQ(&PageTable::current(), &m_page_table);
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
		}
		else if	(m_type == Type::SHARED)
		{
			LockGuard _(m_inode->m_lock);
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

				CriticalScope _;
				PageTable::map_fast_page(pages[page_index]);
				memcpy(PageTable::fast_page_as_ptr(), m_shared_data->page_buffer, PAGE_SIZE);
				PageTable::unmap_fast_page();
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

	BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> FileBackedRegion::clone(PageTable& new_page_table)
	{
		ASSERT_NOT_REACHED();
	}

}
