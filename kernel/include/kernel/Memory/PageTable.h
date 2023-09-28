#pragma once

#include <BAN/Errors.h>
#include <kernel/Memory/Types.h>
#include <kernel/SpinLock.h>

namespace Kernel
{

	class PageTable
	{
	public:
		using flags_t = uint16_t;
		enum Flags : flags_t 
		{
			Present			= (1 << 0),
			ReadWrite		= (1 << 1),
			UserSupervisor	= (1 << 2),
			CacheDisable	= (1 << 4),
			Reserved		= (1 << 9),

			Execute			= (1 << 15),
			Used = Present | Reserved,
		};

	public:
		static void initialize();

		static PageTable& kernel();
		static PageTable& current();

		static bool is_valid_pointer(uintptr_t);

		static BAN::ErrorOr<PageTable*> create_userspace();
		~PageTable();

		void unmap_page(vaddr_t);
		void unmap_range(vaddr_t, size_t bytes);

		void map_range_at(paddr_t, vaddr_t, size_t bytes, flags_t);
		void map_page_at(paddr_t, vaddr_t, flags_t);

		paddr_t physical_address_of(vaddr_t) const;
		flags_t get_page_flags(vaddr_t) const;

		bool is_page_free(vaddr_t) const;
		bool is_range_free(vaddr_t, size_t bytes) const;

		bool reserve_page(vaddr_t, bool only_free = true);
		bool reserve_range(vaddr_t, size_t bytes, bool only_free = true);

		vaddr_t reserve_free_page(vaddr_t first_address, vaddr_t last_address = UINTPTR_MAX);
		vaddr_t reserve_free_contiguous_pages(size_t page_count, vaddr_t first_address, vaddr_t last_address = UINTPTR_MAX);

		void load();

		void lock() const { m_lock.lock(); }
		void unlock() const { m_lock.unlock(); }

		void debug_dump();

	private:
		PageTable() = default;
		uint64_t get_page_data(vaddr_t) const;
		void initialize_kernel();
		void map_kernel_memory();
		void invalidate(vaddr_t);

	private:
		paddr_t						m_highest_paging_struct { 0 };
		mutable RecursiveSpinLock	m_lock;
	};

	static constexpr size_t range_page_count(vaddr_t start, size_t bytes)
	{
		size_t first_page = start / PAGE_SIZE;
		size_t last_page = BAN::Math::div_round_up<size_t>(start + bytes, PAGE_SIZE);
		return last_page - first_page;
	}

}
