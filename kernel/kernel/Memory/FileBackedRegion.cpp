#include <kernel/Lock/LockGuard.h>
#include <kernel/Memory/FileBackedRegion.h>
#include <kernel/Memory/Heap.h>

#include <sys/mman.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::UniqPtr<FileBackedRegion>> FileBackedRegion::create(BAN::RefPtr<Inode> inode, PageTable& page_table, off_t offset, size_t size, AddressRange address_range, Type type, PageTable::flags_t flags, int status_flags)
	{
		ASSERT(inode->mode().ifreg());

		if (offset < 0 || offset % PAGE_SIZE || size == 0)
			return BAN::Error::from_errno(EINVAL);

		size_t inode_size_aligned = inode->size();
		if (auto rem = inode_size_aligned % PAGE_SIZE)
			inode_size_aligned += PAGE_SIZE - rem;

		if ((size > inode_size_aligned || static_cast<size_t>(offset) > inode_size_aligned - size))
			return BAN::Error::from_errno(EOVERFLOW);

		auto* region_ptr = new FileBackedRegion(inode, page_table, offset, size, type, flags, status_flags);
		if (region_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto region = BAN::UniqPtr<FileBackedRegion>::adopt(region_ptr);

		TRY(region->initialize(address_range));

		if (type == Type::PRIVATE)
			TRY(region->m_dirty_pages.resize(BAN::Math::div_round_up<size_t>(size, PAGE_SIZE)));

		LockGuard _(inode->m_mutex);
		if (!(region->m_shared_data = inode->m_shared_region.lock()))
		{
			auto shared_data = TRY(BAN::RefPtr<SharedFileData>::create());
			TRY(shared_data->pages.resize(BAN::Math::div_round_up<size_t>(inode->size(), PAGE_SIZE)));
			shared_data->inode = inode;
			inode->m_shared_region = TRY(shared_data->get_weak_ptr());
			region->m_shared_data = BAN::move(shared_data);
		}

		return region;
	}

	FileBackedRegion::FileBackedRegion(BAN::RefPtr<Inode> inode, PageTable& page_table, off_t offset, ssize_t size, Type type, PageTable::flags_t flags, int status_flags)
		: MemoryRegion(page_table, size, type, flags, status_flags)
		, m_inode(inode)
		, m_offset(offset)
	{
	}

	FileBackedRegion::~FileBackedRegion()
	{
		if (m_vaddr == 0)
			return;
		for (paddr_t dirty_page : m_dirty_pages)
			if (dirty_page)
				Heap::get().release_page(dirty_page);
	}

	SharedFileData::~SharedFileData()
	{
		// no-one should be referencing this anymore
		[[maybe_unused]] bool success = mutex.try_lock();
		ASSERT(success);

		for (size_t i = 0; i < pages.size(); i++)
			if (pages[i])
				sync(i);

		mutex.unlock();
	}

	void SharedFileData::sync(size_t page_index)
	{
		ASSERT(mutex.is_locked());

		if (pages[page_index] == 0)
			return;

		PageTable::with_fast_page(pages[page_index], [&] {
			memcpy(page_buffer, PageTable::fast_page_as_ptr(), PAGE_SIZE);
		});

		const size_t write_size = BAN::Math::min<size_t>(PAGE_SIZE, inode->size() - page_index * PAGE_SIZE);
		if (auto ret = inode->write(page_index * PAGE_SIZE, BAN::ConstByteSpan::from(page_buffer).slice(0, write_size)); ret.is_error())
			dwarnln("{}", ret.error());
	}

	BAN::ErrorOr<void> FileBackedRegion::msync(vaddr_t address, size_t size, int flags)
	{
		if (flags != MS_SYNC)
			dprintln("async file backed mmap msync");
		if (m_type != Type::SHARED)
			return {};

		const vaddr_t first_page = address & PAGE_ADDR_MASK;
		const vaddr_t last_page  = BAN::Math::div_round_up<vaddr_t>(address + size, PAGE_SIZE) * PAGE_SIZE;

		LockGuard _(m_shared_data->mutex);
		for (vaddr_t page_addr = first_page; page_addr < last_page; page_addr += PAGE_SIZE)
			if (contains(page_addr))
				m_shared_data->sync((page_addr - m_vaddr) / PAGE_SIZE);

		return {};
	}

	BAN::ErrorOr<bool> FileBackedRegion::allocate_page_containing_impl(vaddr_t address, bool wants_write)
	{
		ASSERT(contains(address));
		ASSERT(m_type == Type::SHARED || m_type == Type::PRIVATE);
		ASSERT(!wants_write || writable());

		const vaddr_t vaddr = address & PAGE_ADDR_MASK;

		const size_t local_page_index  = (vaddr - m_vaddr) / PAGE_SIZE;
		const size_t shared_page_index = local_page_index + m_offset / PAGE_SIZE;

		if (m_page_table.physical_address_of(vaddr) == 0)
		{
			ASSERT(m_shared_data);
			LockGuard _(m_shared_data->mutex);

			bool shared_data_has_correct_page = false;
			if (m_shared_data->pages[shared_page_index] == 0)
			{
				m_shared_data->pages[shared_page_index] = Heap::get().take_free_page();
				if (m_shared_data->pages[shared_page_index] == 0)
					return BAN::Error::from_errno(ENOMEM);

				const size_t offset = (vaddr - m_vaddr) + m_offset;
				ASSERT(offset % 4096 == 0);

				const size_t bytes = BAN::Math::min<size_t>(m_inode->size() - offset, PAGE_SIZE);

				memset(m_shared_data->page_buffer, 0x00, PAGE_SIZE);
				TRY(m_inode->read(offset, BAN::ByteSpan(m_shared_data->page_buffer, bytes)));
				shared_data_has_correct_page = true;

				PageTable::with_fast_page(m_shared_data->pages[shared_page_index], [&] {
					memcpy(PageTable::fast_page_as_ptr(), m_shared_data->page_buffer, PAGE_SIZE);
				});
			}

			if (m_type == Type::PRIVATE && wants_write)
			{
				const paddr_t paddr = Heap::get().take_free_page();
				if (paddr == 0)
					return BAN::Error::from_errno(ENOMEM);
				if (!shared_data_has_correct_page)
				{
					PageTable::with_fast_page(m_shared_data->pages[shared_page_index], [&] {
						memcpy(m_shared_data->page_buffer, PageTable::fast_page_as_ptr(), PAGE_SIZE);
					});
				}
				PageTable::with_fast_page(paddr, [&] {
					memcpy(PageTable::fast_page_as_ptr(), m_shared_data->page_buffer, PAGE_SIZE);
				});
				m_dirty_pages[local_page_index] = paddr;
				m_page_table.map_page_at(paddr, vaddr, m_flags);
			}
			else
			{
				const paddr_t paddr = m_shared_data->pages[shared_page_index];
				auto flags = m_flags;
				if (m_type == Type::PRIVATE)
					flags &= ~PageTable::Flags::ReadWrite;
				m_page_table.map_page_at(paddr, vaddr, flags);
			}
		}
		else
		{
			// page does not need remappings
			if (m_type != Type::PRIVATE || !wants_write)
				return false;
			ASSERT(writable());

			// page is already mapped as writable
			if (m_page_table.get_page_flags(vaddr) & PageTable::Flags::ReadWrite)
				return false;

			const paddr_t paddr = Heap::get().take_free_page();
			if (paddr == 0)
				return BAN::Error::from_errno(ENOMEM);

			ASSERT(m_shared_data);
			LockGuard _(m_shared_data->mutex);
			ASSERT(m_shared_data->pages[shared_page_index]);

			PageTable::with_fast_page(m_shared_data->pages[shared_page_index], [&] {
				memcpy(m_shared_data->page_buffer, PageTable::fast_page_as_ptr(), PAGE_SIZE);
			});
			PageTable::with_fast_page(paddr, [&] {
				memcpy(PageTable::fast_page_as_ptr(), m_shared_data->page_buffer, PAGE_SIZE);
			});
			m_dirty_pages[local_page_index] = paddr;
			m_page_table.map_page_at(paddr, vaddr, m_flags);
		}

		return true;
	}

	BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> FileBackedRegion::clone(PageTable& page_table)
	{
		const size_t aligned_size = (m_size + PAGE_SIZE - 1) & PAGE_ADDR_MASK;
		auto result = TRY(FileBackedRegion::create(m_inode, page_table, m_offset, m_size, { .start = m_vaddr, .end = m_vaddr + aligned_size }, m_type, m_flags, m_status_flags));

		// non-dirty pages can go through demand paging

		for (size_t i = 0; i < m_dirty_pages.size(); i++)
		{
			if (m_dirty_pages[i] == 0)
				continue;

			const vaddr_t vaddr = m_vaddr + i * PAGE_SIZE;

			const paddr_t paddr = Heap::get().take_free_page();
			if (paddr == 0)
				return BAN::Error::from_errno(ENOMEM);

			ASSERT(&m_page_table == &PageTable::current() || &m_page_table == &PageTable::kernel());
			PageTable::with_fast_page(paddr, [&] {
				memcpy(PageTable::fast_page_as_ptr(), reinterpret_cast<void*>(vaddr), PAGE_SIZE);
			});

			result->m_page_table.map_page_at(paddr, vaddr, m_flags);
			result->m_dirty_pages[i] = paddr;
		}

		return BAN::UniqPtr<MemoryRegion>(BAN::move(result));
	}

	BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> FileBackedRegion::split(size_t offset)
	{
		ASSERT(offset && offset < m_size);
		ASSERT(offset % PAGE_SIZE == 0);

		const bool has_dirty_pages = (m_type == Type::PRIVATE);

		BAN::Vector<paddr_t> dirty_pages;
		if (has_dirty_pages)
		{
			TRY(dirty_pages.resize(BAN::Math::div_round_up<size_t>(m_size - offset, PAGE_SIZE)));
			for (size_t i = 0; i < dirty_pages.size(); i++)
				dirty_pages[i] = m_dirty_pages[i + offset / PAGE_SIZE];
		}

		auto* new_region = new FileBackedRegion(m_inode, m_page_table, m_offset + offset, m_size - offset, m_type, m_flags, m_status_flags);
		if (new_region == nullptr)
			return BAN::Error::from_errno(ENOTSUP);
		new_region->m_vaddr = m_vaddr + offset;
		new_region->m_shared_data = m_shared_data;
		new_region->m_dirty_pages = BAN::move(dirty_pages);

		m_size = offset;
		if (has_dirty_pages)
			MUST(m_dirty_pages.resize(offset / PAGE_SIZE));

		return BAN::UniqPtr<MemoryRegion>::adopt(new_region);
	}

}
