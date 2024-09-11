#include <kernel/Memory/MemoryRegion.h>

namespace Kernel
{

	MemoryRegion::MemoryRegion(PageTable& page_table, size_t size, Type type, PageTable::flags_t flags)
		: m_page_table(page_table)
		, m_size(size)
		, m_type(type)
		, m_flags(flags)
	{
	}

	MemoryRegion::~MemoryRegion()
	{
		if (m_vaddr)
			m_page_table.unmap_range(m_vaddr, m_size);
	}

	BAN::ErrorOr<void> MemoryRegion::initialize(AddressRange address_range)
	{
		size_t needed_pages = BAN::Math::div_round_up<size_t>(m_size, PAGE_SIZE);
		m_vaddr = m_page_table.reserve_free_contiguous_pages(needed_pages, address_range.start);
		if (m_vaddr == 0)
			return BAN::Error::from_errno(ENOMEM);
		if (m_vaddr + needed_pages * PAGE_SIZE > address_range.end)
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
		if (address + size < m_vaddr)
			return false;
		if (address >= m_vaddr + m_size)
			return false;
		return true;
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

}
