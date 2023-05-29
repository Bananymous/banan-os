#pragma once

#include <BAN/Errors.h>
#include <kernel/Memory/Types.h>
#include <kernel/SpinLock.h>

namespace Kernel
{

	class PageTable
	{
	public:
		using flags_t = uint8_t;
		enum Flags : flags_t 
		{
			Present = 1,
			ReadWrite = 2,
			UserSupervisor = 4,
		};

	public:
		static void initialize();

		static PageTable& kernel();
		static PageTable& current();

		static BAN::ErrorOr<PageTable*> create_userspace();
		~PageTable();

		void identity_map_page(paddr_t, flags_t);
		void identity_map_range(paddr_t, size_t bytes, flags_t);

		void unmap_page(vaddr_t);
		void unmap_range(vaddr_t, size_t bytes);

		void map_page_at(paddr_t, vaddr_t, flags_t);

		paddr_t physical_address_of(vaddr_t) const;
		flags_t get_page_flags(vaddr_t) const;

		bool is_page_free(vaddr_t) const;
		bool is_range_free(vaddr_t, size_t bytes) const;

		vaddr_t get_free_page() const;
		vaddr_t get_free_contiguous_pages(size_t page_count) const;

		void invalidate(vaddr_t);
		void load();

		void lock() const { m_lock.lock(); }
		void unlock() const { m_lock.unlock(); }

	private:
		PageTable() = default;
		uint64_t get_page_data(vaddr_t) const;
		void initialize_kernel();

	private:
		uint64_t*					m_highest_paging_struct { nullptr };
		mutable RecursiveSpinLock	m_lock;
	};

}
