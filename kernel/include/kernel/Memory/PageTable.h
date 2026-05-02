#pragma once

#include <BAN/Errors.h>
#include <BAN/Traits.h>
#include <kernel/Lock/SpinLock.h>
#include <kernel/Memory/Types.h>

namespace Kernel
{

	template<typename F>
	concept with_fast_page_callback = requires(F func)
	{
		requires BAN::is_same_v<decltype(func()), void>;
	};

	template<typename F>
	concept with_fast_page_callback_error = requires(F func)
	{
		requires BAN::is_same_v<decltype(func()), BAN::ErrorOr<void>>;
	};

	class PageTable
	{
		BAN_NON_COPYABLE(PageTable);
		BAN_NON_MOVABLE(PageTable);

	public:
		using flags_t = uint16_t;
		enum Flags : flags_t
		{
			Present			= (1 << 0),
			ReadWrite		= (1 << 1),
			UserSupervisor	= (1 << 2),
			Reserved		= (1 << 9),

			Execute			= (1 << 15),
			Used = Present | Reserved,
		};
		enum MemoryType
		{
			Normal,
			Uncached,
			WriteCombining,
			WriteThrough,
		};

	public:
		static void initialize_fast_page();
		static void initialize_and_load();

		static void enable_cpu_features();

		static PageTable& kernel();
		static PageTable& current() { return *reinterpret_cast<PageTable*>(Processor::get_current_page_table()); }

		static constexpr vaddr_t fast_page()
		{
#if ARCH(x86_64)
			return 0xffffffffbfe00000;
#elif ARCH(i686)
			return 0xffe00000;
#endif
		}

		template<with_fast_page_callback F>
		static void with_fast_page(paddr_t paddr, F callback)
		{
			SpinLockGuard _(s_fast_page_lock);
			map_fast_page(paddr);
			callback();
			unmap_fast_page();
		}

		template<with_fast_page_callback_error F>
		static BAN::ErrorOr<void> with_fast_page(paddr_t paddr, F callback)
		{
			SpinLockGuard _(s_fast_page_lock);
			map_fast_page(paddr);
			auto ret = callback();
			unmap_fast_page();
			return ret;
		}

		// FIXME: implement sized checks, return span, etc
		static void* fast_page_as_ptr(size_t offset = 0)
		{
			ASSERT(offset <= PAGE_SIZE);
			return reinterpret_cast<void*>(fast_page() + offset);
		}

		template<typename T>
		static T& fast_page_as(size_t offset = 0)
		{
			ASSERT(offset + sizeof(T) <= PAGE_SIZE);
			return *reinterpret_cast<T*>(fast_page() + offset);
		}

		// Retrieves index'th element from fast_page
		template<typename T>
		static T& fast_page_as_sized(size_t index)
		{
			ASSERT((index + 1) * sizeof(T) <= PAGE_SIZE);
			return *reinterpret_cast<T*>(fast_page() + index * sizeof(T));
		}

		static bool is_valid_pointer(uintptr_t);

		static BAN::ErrorOr<PageTable*> create_userspace();
		~PageTable();

		void unmap_page(vaddr_t, bool invalidate = true);
		void unmap_range(vaddr_t, size_t bytes);

		void map_page_at(paddr_t, vaddr_t, flags_t, MemoryType = MemoryType::Normal, bool invalidate = true);
		void map_range_at(paddr_t, vaddr_t, size_t bytes, flags_t, MemoryType = MemoryType::Normal);

		void remove_writable_from_range(vaddr_t, size_t);

		paddr_t physical_address_of(vaddr_t) const;
		flags_t get_page_flags(vaddr_t) const;

		bool is_page_free(vaddr_t) const;
		bool is_range_free(vaddr_t, size_t bytes) const;

		bool reserve_page(vaddr_t, bool only_free = true, bool invalidate = true);
		bool reserve_range(vaddr_t, size_t bytes, bool only_free = true);

		vaddr_t reserve_free_page(vaddr_t first_address, vaddr_t last_address = UINTPTR_MAX);
		vaddr_t reserve_free_contiguous_pages(size_t page_count, vaddr_t first_address, vaddr_t last_address = UINTPTR_MAX);

		void load();

		void invalidate_page(vaddr_t addr, bool send_smp_message) { invalidate_range(addr, 1, send_smp_message); }
		void invalidate_range(vaddr_t addr, size_t pages, bool send_smp_message);

		InterruptState lock() const { return m_lock.lock(); }
		void unlock(InterruptState state) const { m_lock.unlock(state); }

		paddr_t paddr() const { return m_highest_paging_struct; }

		void debug_dump();

	private:
		PageTable() = default;
		uint64_t get_page_data(vaddr_t) const;
		void map_kernel_memory();

		static void map_fast_page(paddr_t);
		static void unmap_fast_page();

	private:
		paddr_t						m_highest_paging_struct { 0 };
		mutable RecursiveSpinLock	m_lock;
		static SpinLock				s_fast_page_lock;
	};

	static constexpr size_t range_page_count(vaddr_t start, size_t bytes)
	{
		size_t first_page = start / PAGE_SIZE;
		size_t last_page = BAN::Math::div_round_up<size_t>(start + bytes, PAGE_SIZE);
		return last_page - first_page;
	}

}
