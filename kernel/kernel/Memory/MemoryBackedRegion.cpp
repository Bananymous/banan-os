#include <kernel/Memory/Heap.h>
#include <kernel/Memory/MemoryBackedRegion.h>
#include <kernel/Lock/LockGuard.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::UniqPtr<MemoryBackedRegion>> MemoryBackedRegion::create(PageTable& page_table, size_t size, AddressRange address_range, Type type, PageTable::flags_t flags, int status_flags)
	{
		if (type != Type::PRIVATE)
			return BAN::Error::from_errno(ENOTSUP);

		auto* region_ptr = new MemoryBackedRegion(page_table, size, type, flags, status_flags);
		if (region_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto region = BAN::UniqPtr<MemoryBackedRegion>::adopt(region_ptr);

		const size_t page_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
		TRY(region->m_physical_pages.resize(page_count, nullptr));

		TRY(region->initialize(address_range));

		return region;
	}

	MemoryBackedRegion::MemoryBackedRegion(PageTable& page_table, size_t size, Type type, PageTable::flags_t flags, int status_flags)
		: MemoryRegion(page_table, size, type, flags, status_flags)
	{
	}

	MemoryBackedRegion::~MemoryBackedRegion()
	{
		ASSERT(m_type == Type::PRIVATE);

		for (auto* page : m_physical_pages)
			if (page && --page->ref_count == 0)
				delete page;
	}

	MemoryBackedRegion::PhysicalPage::~PhysicalPage()
	{
		Heap::get().release_page(paddr);
	}

	BAN::ErrorOr<bool> MemoryBackedRegion::allocate_page_containing_impl(vaddr_t address, bool wants_write)
	{
		LockGuard _(m_mutex);

		ASSERT(m_type == Type::PRIVATE);
		ASSERT(contains(address));

		const vaddr_t vaddr = address & PAGE_ADDR_MASK;

		auto& physical_page = m_physical_pages[(vaddr - m_vaddr) / PAGE_SIZE];

		if (physical_page == nullptr)
		{
			const paddr_t paddr = Heap::get().take_free_page();
			if (paddr == 0)
				return BAN::Error::from_errno(ENOMEM);

			physical_page = new PhysicalPage(paddr);
			if (physical_page == nullptr)
			{
				Heap::get().release_page(paddr);
				return BAN::Error::from_errno(ENOMEM);
			}

			PageTable::with_per_cpu_fast_page(paddr, [](void* addr) {
				memset(addr, 0x00, PAGE_SIZE);
			});
			m_page_table.map_page_at(paddr, vaddr, m_flags);

			return true;
		}

		if (const bool is_only_ref = (physical_page->ref_count == 1); is_only_ref || !wants_write)
		{
			auto flags = m_flags;
			if (!is_only_ref)
				flags &= ~PageTable::ReadWrite;

			m_page_table.map_page_at(physical_page->paddr, vaddr, flags);

			return true;
		}

		const paddr_t paddr = Heap::get().take_free_page();
		if (paddr == 0)
			return BAN::Error::from_errno(ENOMEM);

		auto* new_physical_page = new PhysicalPage(paddr);
		if (new_physical_page == nullptr)
		{
			Heap::get().release_page(paddr);
			return BAN::Error::from_errno(ENOMEM);
		}

		// NOTE: This is kinda ugly but I don't have support for two per cpu fast pages
		PageTable::with_fast_page(physical_page->paddr, [paddr] {
			PageTable::with_per_cpu_fast_page(paddr, [](void* addr) {
				memcpy(addr, PageTable::fast_page_as_ptr(), PAGE_SIZE);
			});
		});
		m_page_table.map_page_at(paddr, vaddr, m_flags);

		if (--physical_page->ref_count == 0)
			delete physical_page;
		physical_page = new_physical_page;

		return true;
	}

	BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> MemoryBackedRegion::clone(PageTable& new_page_table)
	{
		LockGuard _(m_mutex);

		ASSERT(&PageTable::current() == &m_page_table);

		const size_t aligned_size = (m_size + PAGE_SIZE - 1) & PAGE_ADDR_MASK;
		auto result = TRY(MemoryBackedRegion::create(new_page_table, m_size, { .start = m_vaddr, .end = m_vaddr + aligned_size }, m_type, m_flags, m_status_flags));

		if (writable())
			m_page_table.remove_writable_from_range(m_vaddr, m_size);

		for (size_t i = 0; i < m_physical_pages.size(); i++)
		{
			if (m_physical_pages[i] == nullptr)
				continue;
			result->m_physical_pages[i] = m_physical_pages[i];
			result->m_physical_pages[i]->ref_count++;
		}

		return BAN::UniqPtr<MemoryRegion>(BAN::move(result));
	}

	BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> MemoryBackedRegion::split(size_t offset)
	{
		LockGuard _(m_mutex);

		ASSERT(offset && offset < m_size);
		ASSERT(offset % PAGE_SIZE == 0);

		auto* new_region_ptr = new MemoryBackedRegion(m_page_table, m_size - offset, m_type, m_flags, m_status_flags);
		if (new_region_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto new_region = BAN::UniqPtr<MemoryBackedRegion>::adopt(new_region_ptr);

		new_region->m_vaddr = m_vaddr + offset;

		const size_t moved_pages = (m_size - offset + PAGE_SIZE - 1) / PAGE_SIZE;
		TRY(new_region->m_physical_pages.resize(moved_pages));

		const size_t remaining_pages = m_physical_pages.size() - moved_pages;

		for (size_t i = 0; i < moved_pages; i++)
			new_region->m_physical_pages[i] = m_physical_pages[remaining_pages + i];
		MUST(m_physical_pages.resize(remaining_pages));

		m_size = offset;

		return BAN::UniqPtr<MemoryRegion>(BAN::move(new_region));
	}

	BAN::ErrorOr<void> MemoryBackedRegion::copy_data_to_region(size_t offset_into_region, const uint8_t* buffer, size_t buffer_size)
	{
		LockGuard _(m_mutex);

		ASSERT(offset_into_region + buffer_size <= m_size);

		size_t written = 0;
		while (written < buffer_size)
		{
			const vaddr_t write_vaddr = m_vaddr + offset_into_region + written;
			const vaddr_t page_offset = write_vaddr % PAGE_SIZE;
			const size_t bytes = BAN::Math::min<size_t>(buffer_size - written, PAGE_SIZE - page_offset);

			if (!(m_page_table.get_page_flags(write_vaddr & PAGE_ADDR_MASK) & PageTable::ReadWrite))
			{
				if (!TRY(allocate_page_containing(write_vaddr, true)))
				{
					dwarnln("Could not allocate page for data copying");
					return BAN::Error::from_errno(EFAULT);
				}
			}

			const paddr_t paddr = m_page_table.physical_address_of(write_vaddr & PAGE_ADDR_MASK);
			ASSERT(paddr);

			PageTable::with_per_cpu_fast_page(paddr, [&](void* addr) {
				memcpy(static_cast<uint8_t*>(addr) + page_offset, (void*)(buffer + written), bytes);
			});

			written += bytes;
		}

		return {};
	}

	BAN::ErrorOr<void> MemoryBackedRegion::merge_with(MemoryBackedRegion& region)
	{
		LockGuard _0(m_mutex);
		LockGuard _1(region.m_mutex);

		ASSERT(flags() == region.flags());
		ASSERT(type()  == region.type());

		ASSERT(region.vaddr() == ((m_vaddr + m_size + PAGE_SIZE - 1) & PAGE_ADDR_MASK));

		const size_t old_pages = m_physical_pages.size();
		TRY(m_physical_pages.resize(old_pages + region.m_physical_pages.size()));
		for (size_t i = 0; i < region.m_physical_pages.size(); i++)
			m_physical_pages[old_pages + i] = region.m_physical_pages[i];

		m_size = (region.vaddr() + region.size()) - m_vaddr;

		region.m_vaddr = 0;
		region.m_size  = 0;
		region.m_physical_pages.clear();

		return {};
	}

}
