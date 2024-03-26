#include <kernel/Memory/PageTable.h>
#include <kernel/Lock/SpinLock.h>

namespace Kernel
{

	RecursiveSpinLock PageTable::s_fast_page_lock;

	void PageTable::initialize()
	{
		ASSERT_NOT_REACHED();
	}

	PageTable& PageTable::kernel()
	{
		ASSERT_NOT_REACHED();
	}

	bool PageTable::is_valid_pointer(uintptr_t)
	{
		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<PageTable*> PageTable::create_userspace()
	{
		ASSERT_NOT_REACHED();
	}

	PageTable::~PageTable()
	{
		ASSERT_NOT_REACHED();
	}

	void PageTable::unmap_page(vaddr_t)
	{
		ASSERT_NOT_REACHED();
	}

	void PageTable::unmap_range(vaddr_t, size_t)
	{
		ASSERT_NOT_REACHED();
	}

	void PageTable::map_range_at(paddr_t, vaddr_t, size_t, flags_t)
	{
		ASSERT_NOT_REACHED();
	}

	void PageTable::map_page_at(paddr_t, vaddr_t, flags_t)
	{
		ASSERT_NOT_REACHED();
	}

	paddr_t PageTable::physical_address_of(vaddr_t) const
	{
		ASSERT_NOT_REACHED();
	}

	PageTable::flags_t PageTable::get_page_flags(vaddr_t) const
	{
		ASSERT_NOT_REACHED();
	}

	bool PageTable::is_page_free(vaddr_t) const
	{
		ASSERT_NOT_REACHED();
	}

	bool PageTable::is_range_free(vaddr_t, size_t) const
	{
		ASSERT_NOT_REACHED();
	}

	bool PageTable::reserve_page(vaddr_t, bool)
	{
		ASSERT_NOT_REACHED();
	}

	bool PageTable::reserve_range(vaddr_t, size_t, bool)
	{
		ASSERT_NOT_REACHED();
	}

	vaddr_t PageTable::reserve_free_page(vaddr_t, vaddr_t)
	{
		ASSERT_NOT_REACHED();
	}

	vaddr_t PageTable::reserve_free_contiguous_pages(size_t, vaddr_t, vaddr_t)
	{
		ASSERT_NOT_REACHED();
	}

	void PageTable::load()
	{
		ASSERT_NOT_REACHED();
	}

	void PageTable::initial_load()
	{
		ASSERT_NOT_REACHED();
	}

	void PageTable::debug_dump()
	{
		ASSERT_NOT_REACHED();
	}

	uint64_t PageTable::get_page_data(vaddr_t) const
	{
		ASSERT_NOT_REACHED();
	}

	void PageTable::initialize_kernel()
	{
		ASSERT_NOT_REACHED();
	}

	void PageTable::map_kernel_memory()
	{
		ASSERT_NOT_REACHED();
	}

	void PageTable::prepare_fast_page()
	{
		ASSERT_NOT_REACHED();
	}

	void PageTable::invalidate(vaddr_t)
	{
		ASSERT_NOT_REACHED();
	}

	void PageTable::map_fast_page(paddr_t)
	{
		ASSERT_NOT_REACHED();
	}

	void PageTable::unmap_fast_page()
	{
		ASSERT_NOT_REACHED();
	}

}
