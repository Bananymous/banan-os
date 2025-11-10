#include <kernel/Lock/LockGuard.h>
#include <kernel/Memory/MemoryRegion.h>

namespace Kernel
{

	MemoryRegion::MemoryRegion(PageTable& page_table, size_t size, Type type, PageTable::flags_t flags, int status_flags)
		: m_page_table(page_table)
		, m_size(size)
		, m_type(type)
		, m_flags(flags)
		, m_status_flags(status_flags)
	{
	}

	MemoryRegion::~MemoryRegion()
	{
		ASSERT(m_pinned_count == 0);
		if (m_vaddr)
			m_page_table.unmap_range(m_vaddr, m_size);
	}

	BAN::ErrorOr<void> MemoryRegion::initialize(AddressRange address_range)
	{
		size_t needed_pages = BAN::Math::div_round_up<size_t>(m_size, PAGE_SIZE);
		m_vaddr = m_page_table.reserve_free_contiguous_pages(needed_pages, address_range.start);
		if (m_vaddr == 0)
			return BAN::Error::from_errno(ENOMEM);
		if (m_vaddr + m_size > address_range.end)
			return BAN::Error::from_errno(ENOMEM);
		return {};
	}

	bool MemoryRegion::contains(vaddr_t address) const
	{
		return m_vaddr <= address && address < m_vaddr + m_size;
	}

	bool MemoryRegion::contains_fully(vaddr_t address, size_t size) const
	{
		return m_vaddr <= address && address + size <= m_vaddr + m_size;
	}

	bool MemoryRegion::overlaps(vaddr_t address, size_t size) const
	{
		if (address + size <= m_vaddr)
			return false;
		if (address >= m_vaddr + m_size)
			return false;
		return true;
	}

	bool MemoryRegion::is_contained_by(vaddr_t address, size_t size) const
	{
		return address <= m_vaddr && m_vaddr + m_size <= address + size;
	}

	BAN::ErrorOr<void> MemoryRegion::mprotect(PageTable::flags_t new_page_flags)
	{
		if (m_flags == new_page_flags)
			return {};

		const size_t page_count = BAN::Math::div_round_up<size_t>(m_size, PAGE_SIZE);
		for (size_t i = 0; i < page_count; i++)
		{
			const vaddr_t vaddr = m_vaddr + i * PAGE_SIZE;
			const paddr_t paddr = m_page_table.physical_address_of(vaddr);
			if (paddr == 0)
				continue;
			m_page_table.map_page_at(paddr, vaddr, new_page_flags);
		}

		m_flags = new_page_flags;
		return {};
	}

	BAN::ErrorOr<bool> MemoryRegion::allocate_page_containing(vaddr_t address, bool wants_write)
	{
		ASSERT(contains(address));
		if (wants_write && !writable())
			return false;
		auto ret = allocate_page_containing_impl(address, wants_write);
		if (!ret.is_error() && ret.value())
			m_physical_page_count++;
		return ret;
	}

	void MemoryRegion::pin()
	{
		LockGuard _(m_pinned_mutex);
		m_pinned_count++;
	}

	void MemoryRegion::unpin()
	{
		LockGuard _(m_pinned_mutex);
		if (--m_pinned_count == 0)
			m_pinned_blocker.unblock();
	}

	void MemoryRegion::wait_not_pinned()
	{
		LockGuard _(m_pinned_mutex);
		while (m_pinned_count)
			m_pinned_blocker.block_with_timeout_ms(100, &m_pinned_mutex);
	}

}
