#include <kernel/BootInfo.h>
#include <kernel/CPUID.h>
#include <kernel/Lock/SpinLock.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/PageTable.h>

extern uint8_t g_kernel_start[];
extern uint8_t g_kernel_end[];

extern uint8_t g_kernel_execute_start[];
extern uint8_t g_kernel_execute_end[];

extern uint8_t g_kernel_writable_start[];
extern uint8_t g_kernel_writable_end[];

extern uint8_t g_userspace_start[];
extern uint8_t g_userspace_end[];

extern uint64_t g_boot_fast_page_pt[];

namespace Kernel
{

	SpinLock PageTable::s_fast_page_lock;

	static constexpr vaddr_t s_hhdm_offset = 0xFFFF800000000000;
	static bool s_is_initialized = false;

	constexpr uint64_t s_page_flag_mask = 0x8000000000000FFF;
	constexpr uint64_t s_page_addr_mask = ~s_page_flag_mask;

	static PageTable* s_kernel = nullptr;
	static bool s_has_nxe = false;
	static bool s_has_pge = false;
	static bool s_has_gib = false;

	static paddr_t s_global_pml4_entries[512] { 0 };

	static uint64_t* s_fast_page_pt { nullptr };

	static constexpr inline bool is_canonical(uintptr_t addr)
	{
		constexpr uintptr_t mask = 0xFFFF800000000000;
		addr &= mask;
		return addr == mask || addr == 0;
	}

	static constexpr inline uintptr_t uncanonicalize(uintptr_t addr)
	{
		return addr & 0x0000FFFFFFFFFFFF;
	}

	static constexpr inline uintptr_t canonicalize(uintptr_t addr)
	{
		if (addr & 0x0000800000000000)
			return addr | 0xFFFF000000000000;
		return addr;
	}

	static paddr_t allocate_zeroed_page_aligned_page()
	{
		const paddr_t paddr = Heap::get().take_free_page();
		ASSERT(paddr);
		memset(reinterpret_cast<void*>(paddr + s_hhdm_offset), 0, PAGE_SIZE);
		return paddr;
	}

	static void unallocate_page(paddr_t paddr)
	{
		Heap::get().release_page(paddr);
	}

	static uint64_t* P2V(paddr_t paddr)
	{
		ASSERT(paddr != 0);
		ASSERT(!BAN::Math::will_addition_overflow(paddr, s_hhdm_offset));
		return reinterpret_cast<uint64_t*>(paddr + s_hhdm_offset);
	}

	static PageTable::flags_t parse_flags(uint64_t entry)
	{
		using Flags = PageTable::Flags;

		PageTable::flags_t result = 0;
		if (s_has_nxe && !(entry & (1ull << 63)))
			result |= Flags::Execute;
		if (entry & Flags::Reserved)
			result |= Flags::Reserved;
		if (entry & Flags::UserSupervisor)
			result |= Flags::UserSupervisor;
		if (entry & Flags::ReadWrite)
			result |= Flags::ReadWrite;
		if (entry & Flags::Present)
			result |= Flags::Present;
		return result;
	}

	// page size:
	//   0: 4 KiB
	//   1: 2 MiB
	//   2: 1 GiB
	static void map_hhdm_page(paddr_t pml4, paddr_t paddr, uint8_t page_size)
	{
		ASSERT(0 <= page_size && page_size <= 2);

		const vaddr_t vaddr = paddr + s_hhdm_offset;
		ASSERT(vaddr < KERNEL_OFFSET);

		const vaddr_t uc_vaddr = uncanonicalize(vaddr);
		const uint16_t pml4e = (uc_vaddr >> 39) & 0x1FF;
		const uint16_t pdpte = (uc_vaddr >> 30) & 0x1FF;
		const uint16_t pde   = (uc_vaddr >> 21) & 0x1FF;
		const uint16_t pte   = (uc_vaddr >> 12) & 0x1FF;

		static constexpr uint64_t hhdm_flags = (1u << 1) | (1u << 0);

		const auto get_or_allocate_entry =
			[](paddr_t table, uint16_t table_entry, uint64_t extra_flags) -> paddr_t
			{
				paddr_t result = 0;
				PageTable::with_fast_page(table, [&] {
					const uint64_t entry = PageTable::fast_page_as_sized<uint64_t>(table_entry);
					if (entry & (1u << 0))
						result = entry & s_page_addr_mask;
				});
				if (result != 0)
					return result;

				const paddr_t new_paddr = Heap::get().take_free_page();
				ASSERT(new_paddr);

				PageTable::with_fast_page(new_paddr, [] {
					memset(reinterpret_cast<void*>(PageTable::fast_page_as_ptr()), 0, PAGE_SIZE);
				});

				PageTable::with_fast_page(table, [&] {
					uint64_t& entry = PageTable::fast_page_as_sized<uint64_t>(table_entry);
					entry = new_paddr | hhdm_flags | extra_flags;
				});

				return new_paddr;
			};

		const uint64_t pgsize_flag = page_size ? (static_cast<uint64_t>(1) <<  7) : 0;
		const uint64_t global_flag = s_has_pge ? (static_cast<uint64_t>(1) <<  8) : 0;
		const uint64_t noexec_flag = s_has_nxe ? (static_cast<uint64_t>(1) << 63) : 0;

		const paddr_t pdpt = get_or_allocate_entry(pml4, pml4e, noexec_flag);
		s_global_pml4_entries[pml4e] = pdpt | hhdm_flags | noexec_flag;

		paddr_t lowest_paddr = pdpt;
		uint16_t lowest_entry = pdpte;

		if (page_size < 2)
		{
			lowest_paddr = get_or_allocate_entry(lowest_paddr, lowest_entry, noexec_flag);
			lowest_entry = pde;
		}

		if (page_size < 1)
		{
			lowest_paddr = get_or_allocate_entry(lowest_paddr, lowest_entry, noexec_flag);
			lowest_entry = pte;
		}

		PageTable::with_fast_page(lowest_paddr, [&] {
			uint64_t& entry = PageTable::fast_page_as_sized<uint64_t>(lowest_entry);
			entry = paddr | hhdm_flags | noexec_flag | global_flag | pgsize_flag;
		});
	}

	static void initialize_hhdm(paddr_t pml4)
	{
		for (const auto& entry : g_boot_info.memory_map_entries)
		{
			if (entry.type != MemoryMapEntry::Type::Available)
				continue;

			constexpr size_t one_gib = 1024 * 1024 * 1024;
			constexpr size_t two_mib =    2 * 1024 * 1024;

			const paddr_t entry_start = (entry.address + PAGE_SIZE - 1) & PAGE_ADDR_MASK;
			const paddr_t entry_end   = (entry.address + entry.length)  & PAGE_ADDR_MASK;
			for (paddr_t paddr = entry_start; paddr < entry_end;)
			{
				if (s_has_gib && paddr % one_gib == 0 && paddr + one_gib <= entry_end)
				{
					map_hhdm_page(pml4, paddr, 2);
					paddr += one_gib;
				}
				else if (paddr % two_mib == 0 && paddr + two_mib <= entry_end)
				{
					map_hhdm_page(pml4, paddr, 1);
					paddr += two_mib;
				}
				else
				{
					map_hhdm_page(pml4, paddr, 0);
					paddr += PAGE_SIZE;
				}
			}
		}
	}

	void PageTable::initialize_fast_page()
	{
		s_fast_page_pt = g_boot_fast_page_pt;
	}

	static void detect_cpu_features()
	{
		if (CPUID::has_nxe())
			s_has_nxe = true;
		if (CPUID::has_pge())
			s_has_pge = true;
		if (CPUID::has_1gib_pages())
			s_has_gib = true;
	}

	void PageTable::enable_cpu_features()
	{
		if (s_has_nxe)
		{
			asm volatile(
				"movl $0xC0000080, %%ecx;"
				"rdmsr;"
				"orl $0x800, %%eax;"
				"wrmsr"
				::: "eax", "ecx", "edx", "memory"
			);
		}

		if (s_has_pge)
		{
			asm volatile(
				"movq %%cr4, %%rax;"
				"orq $0x80, %%rax;"
				"movq %%rax, %%cr4;"
				::: "rax"
			);
		}

		// 64-bit always has PAT, set PAT4 = WC, PAT5 = WT
		asm volatile(
			"movl $0x277, %%ecx;"
			"rdmsr;"
			"movw $0x0401, %%dx;"
			"wrmsr;"
			::: "eax", "ecx", "edx", "memory"
		);

		// enable write protect
		asm volatile(
			"movq %%cr0, %%rax;"
			"orq $0x10000, %%rax;"
			"movq %%rax, %%cr0;"
			::: "rax"
		);
	}

	void PageTable::initialize_and_load()
	{
		detect_cpu_features();
		enable_cpu_features();

		const paddr_t boot_pml4_paddr = ({
			paddr_t paddr;
			asm volatile("movq %%cr3, %0" : "=r"(paddr));
			paddr;
		});

		initialize_hhdm(boot_pml4_paddr);

		ASSERT(s_kernel == nullptr);
		s_kernel = new PageTable();
		ASSERT(s_kernel != nullptr);

		s_kernel->m_highest_paging_struct = allocate_zeroed_page_aligned_page();
		ASSERT(s_kernel->m_highest_paging_struct);

		uint64_t* pml4 = P2V(s_kernel->m_highest_paging_struct);
		memcpy(pml4, s_global_pml4_entries, sizeof(s_global_pml4_entries));
		s_kernel->map_kernel_memory();
		s_global_pml4_entries[511] = pml4[511];

		// update fast page pt
		{
			constexpr vaddr_t uc_vaddr = uncanonicalize(fast_page());
			constexpr uint16_t pml4e = (uc_vaddr >> 39) & 0x1FF;
			constexpr uint16_t pdpte = (uc_vaddr >> 30) & 0x1FF;
			constexpr uint16_t pde   = (uc_vaddr >> 21) & 0x1FF;

			const auto get_or_allocate_entry =
				[](paddr_t table_paddr, uint16_t entry, uint64_t flags)
				{
					uint64_t* table = P2V(table_paddr);

					if (!(table[entry] & Flags::Present))
					{
						table[entry] = allocate_zeroed_page_aligned_page();
						ASSERT(table[entry]);
					}

					table[entry] |= flags;

					return table[entry] & s_page_addr_mask;
				};

			const paddr_t pml4 = s_kernel->m_highest_paging_struct;
			const paddr_t pdpt = get_or_allocate_entry(pml4, pml4e, Flags::ReadWrite | Flags::Present);
			const paddr_t pd   = get_or_allocate_entry(pdpt, pdpte, Flags::ReadWrite | Flags::Present);
			s_fast_page_pt = P2V(get_or_allocate_entry(pd,   pde,   Flags::ReadWrite | Flags::Present));
		}

		s_kernel->load();
	}

	PageTable& PageTable::kernel()
	{
		ASSERT(s_kernel);
		return *s_kernel;
	}

	bool PageTable::is_valid_pointer(uintptr_t pointer)
	{
		if (!is_canonical(pointer))
			return false;
		return true;
	}

	void PageTable::map_kernel_memory()
	{
		// Map (phys_kernel_start -> phys_kernel_end) to (virt_kernel_start -> virt_kernel_end)
		const vaddr_t kernel_start = reinterpret_cast<vaddr_t>(g_kernel_start);
		map_range_at(
			kernel_start - KERNEL_OFFSET + g_boot_info.kernel_paddr,
			kernel_start,
			g_kernel_end - g_kernel_start,
			Flags::Present
		);

		// Map executable kernel memory as executable
		const vaddr_t kernel_execute_start = reinterpret_cast<vaddr_t>(g_kernel_execute_start);
		map_range_at(
			kernel_execute_start - KERNEL_OFFSET + g_boot_info.kernel_paddr,
			kernel_execute_start,
			g_kernel_execute_end - g_kernel_execute_start,
			Flags::Execute | Flags::Present
		);

		// Map writable kernel memory as writable
		const vaddr_t kernel_writable_start = reinterpret_cast<vaddr_t>(g_kernel_writable_start);
		map_range_at(
			kernel_writable_start - KERNEL_OFFSET + g_boot_info.kernel_paddr,
			kernel_writable_start,
			g_kernel_writable_end - g_kernel_writable_start,
			Flags::ReadWrite | Flags::Present
		);

		// Map userspace memory
		const vaddr_t userspace_start = reinterpret_cast<vaddr_t>(g_userspace_start);
		map_range_at(
			userspace_start - KERNEL_OFFSET + g_boot_info.kernel_paddr,
			userspace_start,
			g_userspace_end - g_userspace_start,
			Flags::Execute | Flags::UserSupervisor | Flags::Present
		);
	}

	void PageTable::map_fast_page(paddr_t paddr)
	{
		ASSERT(paddr && paddr % PAGE_SIZE == 0);

		ASSERT(s_fast_page_pt);
		ASSERT(s_fast_page_lock.current_processor_has_lock());

		ASSERT(!(*s_fast_page_pt & Flags::Present));
		s_fast_page_pt[0] = paddr | Flags::ReadWrite | Flags::Present;

		asm volatile("invlpg (%0)" :: "r"(fast_page()));
	}

	void PageTable::unmap_fast_page()
	{
		ASSERT(s_fast_page_pt);
		ASSERT(s_fast_page_lock.current_processor_has_lock());

		ASSERT((*s_fast_page_pt & Flags::Present));
		s_fast_page_pt[0] = 0;

		asm volatile("invlpg (%0)" :: "r"(fast_page()));
	}

	BAN::ErrorOr<PageTable*> PageTable::create_userspace()
	{
		SpinLockGuard _(s_kernel->m_lock);

		PageTable* page_table = new PageTable;
		if (page_table == nullptr)
			return BAN::Error::from_errno(ENOMEM);

		page_table->m_highest_paging_struct = allocate_zeroed_page_aligned_page();
		if (page_table->m_highest_paging_struct == 0)
		{
			delete page_table;
			return BAN::Error::from_errno(ENOMEM);
		}

		uint64_t* pml4 = P2V(page_table->m_highest_paging_struct);
		memcpy(pml4, s_global_pml4_entries, sizeof(s_global_pml4_entries));

		return page_table;
	}

	PageTable::~PageTable()
	{
		if (m_highest_paging_struct == 0)
			return;

		// NOTE: we only loop until 256 since after that is hhdm
		const uint64_t* pml4 = P2V(m_highest_paging_struct);
		for (uint64_t pml4e = 0; pml4e < 256; pml4e++)
		{
			if (!(pml4[pml4e] & Flags::Present))
				continue;
			const uint64_t* pdpt = P2V(pml4[pml4e] & s_page_addr_mask);
			for (uint64_t pdpte = 0; pdpte < 512; pdpte++)
			{
				if (!(pdpt[pdpte] & Flags::Present))
					continue;
				const uint64_t* pd = P2V(pdpt[pdpte] & s_page_addr_mask);
				for (uint64_t pde = 0; pde < 512; pde++)
				{
					if (!(pd[pde] & Flags::Present))
						continue;
					unallocate_page(pd[pde] & s_page_addr_mask);
				}
				unallocate_page(pdpt[pdpte] & s_page_addr_mask);
			}
			unallocate_page(pml4[pml4e] & s_page_addr_mask);
		}
		unallocate_page(m_highest_paging_struct);
	}

	void PageTable::load()
	{
		SpinLockGuard _(m_lock);
		asm volatile("movq %0, %%cr3" :: "r"(m_highest_paging_struct));
		Processor::set_current_page_table(this);
	}

	void PageTable::invalidate_range(vaddr_t vaddr, size_t pages, bool send_smp_message)
	{
		ASSERT(vaddr % PAGE_SIZE == 0);

		const bool is_userspace = (vaddr < KERNEL_OFFSET);
		if (is_userspace && this != &PageTable::current())
			;
		else if (pages <= 32 || !s_is_initialized)
		{
			for (size_t i = 0; i < pages; i++, vaddr += PAGE_SIZE)
				asm volatile("invlpg (%0)" :: "r"(vaddr));
		}
		else if (is_userspace || !s_has_pge)
		{
			asm volatile("movq %0, %%cr3" :: "r"(m_highest_paging_struct));
		}
		else
		{
			asm volatile(
				"movq %%cr4, %%rax;"

				"andq $~0x80, %%rax;"
				"movq %%rax, %%cr4;"

				"movq %0, %%cr3;"

				"orq $0x80, %%rax;"
				"movq %%rax, %%cr4;"
				:
				: "r"(m_highest_paging_struct)
				: "rax"
			);
		}

		if (send_smp_message)
		{
			Processor::broadcast_smp_message({
				.type = Processor::SMPMessage::Type::FlushTLB,
				.flush_tlb = {
					.vaddr      = vaddr,
					.page_count = pages,
					.page_table = vaddr < KERNEL_OFFSET ? this : nullptr,
				}
			});
		}
	}

	void PageTable::unmap_page(vaddr_t vaddr, bool invalidate)
	{
		ASSERT(vaddr);
		ASSERT(vaddr != fast_page());
		if (vaddr >= KERNEL_OFFSET)
			ASSERT(vaddr >= (vaddr_t)g_kernel_start);
		if ((vaddr >= KERNEL_OFFSET) != (this == s_kernel))
			Kernel::panic("unmapping {8H}, kernel: {}", vaddr, this == s_kernel);

		ASSERT(is_canonical(vaddr));
		const vaddr_t uc_vaddr = uncanonicalize(vaddr);

		ASSERT(vaddr % PAGE_SIZE == 0);

		const uint16_t pml4e = (uc_vaddr >> 39) & 0x1FF;
		const uint16_t pdpte = (uc_vaddr >> 30) & 0x1FF;
		const uint16_t pde   = (uc_vaddr >> 21) & 0x1FF;
		const uint16_t pte   = (uc_vaddr >> 12) & 0x1FF;

		SpinLockGuard _(m_lock);

		if (is_page_free(vaddr))
			Kernel::panic("trying to unmap unmapped page 0x{H}", vaddr);

		uint64_t* pml4 = P2V(m_highest_paging_struct);
		uint64_t* pdpt = P2V(pml4[pml4e] & s_page_addr_mask);
		uint64_t* pd   = P2V(pdpt[pdpte] & s_page_addr_mask);
		uint64_t* pt   = P2V(pd[pde]     & s_page_addr_mask);

		const paddr_t old_paddr = pt[pte] & PAGE_ADDR_MASK;

		pt[pte] = 0;

		if (invalidate && old_paddr != 0)
			invalidate_page(vaddr, true);
	}

	void PageTable::unmap_range(vaddr_t vaddr, size_t size)
	{
		ASSERT(vaddr % PAGE_SIZE == 0);

		const size_t page_count = range_page_count(vaddr, size);

		SpinLockGuard _(m_lock);
		for (vaddr_t page = 0; page < page_count; page++)
			unmap_page(vaddr + page * PAGE_SIZE, false);
		invalidate_range(vaddr, page_count, true);
	}

	void PageTable::map_page_at(paddr_t paddr, vaddr_t vaddr, flags_t flags, MemoryType memory_type, bool invalidate)
	{
		ASSERT(vaddr);
		ASSERT(vaddr != fast_page());
		if (vaddr < KERNEL_OFFSET && this == s_kernel)
			panic("kernel is mapping below kernel offset");
		if (vaddr >= s_hhdm_offset && this != s_kernel)
			panic("user is mapping above hhdm offset");

		ASSERT(is_canonical(vaddr));
		const vaddr_t uc_vaddr = uncanonicalize(vaddr);

		ASSERT(paddr % PAGE_SIZE == 0);
		ASSERT(vaddr % PAGE_SIZE == 0);
		ASSERT(flags & Flags::Used);

		const uint16_t pml4e = (uc_vaddr >> 39) & 0x1FF;
		const uint16_t pdpte = (uc_vaddr >> 30) & 0x1FF;
		const uint16_t pde   = (uc_vaddr >> 21) & 0x1FF;
		const uint16_t pte   = (uc_vaddr >> 12) & 0x1FF;

		uint64_t extra_flags = 0;
		if (s_has_pge && pml4e == 511) // Map kernel memory as global
			extra_flags |= 1ull << 8;
		if (s_has_nxe && !(flags & Flags::Execute))
			extra_flags |= 1ull << 63;
		if (flags & Flags::Reserved)
			extra_flags |= Flags::Reserved;

		if (memory_type == MemoryType::Uncached)
			extra_flags |= (1ull << 4);
		if (memory_type == MemoryType::WriteCombining)
			extra_flags |= (1ull << 7);
		if (memory_type == MemoryType::WriteThrough)
			extra_flags |= (1ull << 7) | (1ull << 3);

		// NOTE: we add present here, since it has to be available in higher level structures
		flags_t uwr_flags = (flags & (Flags::UserSupervisor | Flags::ReadWrite)) | Flags::Present;

		SpinLockGuard _(m_lock);

		const auto allocate_entry_if_needed =
			[](uint64_t* table, uint16_t index, flags_t flags) -> uint64_t*
			{
				uint64_t entry = table[index];
				if ((entry & flags) == flags)
					return P2V(entry & s_page_addr_mask);
				if (!(entry & Flags::Present))
					entry = allocate_zeroed_page_aligned_page();
				table[index] = entry | flags;
				return P2V(entry & s_page_addr_mask);
			};

		uint64_t* pml4 = P2V(m_highest_paging_struct);
		uint64_t* pdpt = allocate_entry_if_needed(pml4, pml4e, uwr_flags);
		uint64_t* pd   = allocate_entry_if_needed(pdpt, pdpte, uwr_flags);
		uint64_t* pt   = allocate_entry_if_needed(pd,   pde,   uwr_flags);

		if (!(flags & Flags::Present))
			uwr_flags &= ~Flags::Present;

		const paddr_t old_paddr = pt[pte] & PAGE_ADDR_MASK;

		pt[pte] = paddr | uwr_flags | extra_flags;

		if (invalidate && old_paddr != 0)
			invalidate_page(vaddr, true);
	}

	void PageTable::map_range_at(paddr_t paddr, vaddr_t vaddr, size_t size, flags_t flags, MemoryType memory_type)
	{
		ASSERT(is_canonical(vaddr));

		ASSERT(vaddr);
		ASSERT(paddr % PAGE_SIZE == 0);
		ASSERT(vaddr % PAGE_SIZE == 0);

		size_t page_count = range_page_count(vaddr, size);

		SpinLockGuard _(m_lock);
		for (size_t page = 0; page < page_count; page++)
			map_page_at(paddr + page * PAGE_SIZE, vaddr + page * PAGE_SIZE, flags, memory_type, false);
		invalidate_range(vaddr, page_count, true);
	}

	void PageTable::remove_writable_from_range(vaddr_t vaddr, size_t size)
	{
		ASSERT(vaddr);
		ASSERT(vaddr % PAGE_SIZE == 0);

		ASSERT(is_canonical(vaddr));
		ASSERT(is_canonical(vaddr + size - 1));

		const vaddr_t uc_vaddr_start = uncanonicalize(vaddr);
		const vaddr_t uc_vaddr_end   = uncanonicalize(vaddr + size - 1);

		uint16_t pml4e = (uc_vaddr_start >> 39) & 0x1FF;
		uint16_t pdpte = (uc_vaddr_start >> 30) & 0x1FF;
		uint16_t pde   = (uc_vaddr_start >> 21) & 0x1FF;
		uint16_t pte   = (uc_vaddr_start >> 12) & 0x1FF;

		const uint16_t e_pml4e = (uc_vaddr_end >> 39) & 0x1FF;
		const uint16_t e_pdpte = (uc_vaddr_end >> 30) & 0x1FF;
		const uint16_t e_pde   = (uc_vaddr_end >> 21) & 0x1FF;
		const uint16_t e_pte   = (uc_vaddr_end >> 12) & 0x1FF;

		SpinLockGuard _(m_lock);

		const uint64_t* pml4 = P2V(m_highest_paging_struct);
		for (; pml4e <= e_pml4e; pml4e++)
		{
			if (!(pml4[pml4e] & Flags::ReadWrite))
				continue;
			const uint64_t* pdpt = P2V(pml4[pml4e] & s_page_addr_mask);
			for (; pdpte < 512; pdpte++)
			{
				if (pml4e == e_pml4e && pdpte > e_pdpte)
					break;
				if (!(pdpt[pdpte] & Flags::ReadWrite))
					continue;
				const uint64_t* pd = P2V(pdpt[pdpte] & s_page_addr_mask);
				for (; pde < 512; pde++)
				{
					if (pml4e == e_pml4e && pdpte == e_pdpte && pde > e_pde)
						break;
					if (!(pd[pde] & Flags::ReadWrite))
						continue;
					uint64_t* pt = P2V(pd[pde] & s_page_addr_mask);
					for (; pte < 512; pte++)
					{
						if (pml4e == e_pml4e && pdpte == e_pdpte && pde == e_pde && pte > e_pte)
							break;
						pt[pte] &= ~static_cast<uint64_t>(Flags::ReadWrite);
					}
					pte = 0;
				}
				pde = 0;
			}
			pdpte = 0;
		}

		invalidate_range(vaddr, size / PAGE_SIZE, true);
	}

	uint64_t PageTable::get_page_data(vaddr_t vaddr) const
	{
		ASSERT(is_canonical(vaddr));
		const vaddr_t uc_vaddr = uncanonicalize(vaddr);

		ASSERT(vaddr % PAGE_SIZE == 0);

		const uint16_t pml4e = (uc_vaddr >> 39) & 0x1FF;
		const uint16_t pdpte = (uc_vaddr >> 30) & 0x1FF;
		const uint16_t pde   = (uc_vaddr >> 21) & 0x1FF;
		const uint16_t pte   = (uc_vaddr >> 12) & 0x1FF;

		SpinLockGuard _(m_lock);

		const uint64_t* pml4 = P2V(m_highest_paging_struct);
		if (!(pml4[pml4e] & Flags::Present))
			return 0;

		const uint64_t* pdpt = P2V(pml4[pml4e] & s_page_addr_mask);
		if (!(pdpt[pdpte] & Flags::Present))
			return 0;

		const uint64_t* pd = P2V(pdpt[pdpte] & s_page_addr_mask);
		if (!(pd[pde] & Flags::Present))
			return 0;

		const uint64_t* pt = P2V(pd[pde] & s_page_addr_mask);
		if (!(pt[pte] & Flags::Used))
			return 0;

		return pt[pte];
	}

	PageTable::flags_t PageTable::get_page_flags(vaddr_t addr) const
	{
		return parse_flags(get_page_data(addr));
	}

	paddr_t PageTable::physical_address_of(vaddr_t addr) const
	{
		uint64_t page_data = get_page_data(addr);
		return page_data & s_page_addr_mask;
	}

	bool PageTable::reserve_page(vaddr_t vaddr, bool only_free, bool invalidate)
	{
		SpinLockGuard _(m_lock);
		ASSERT(vaddr % PAGE_SIZE == 0);
		if (only_free && !is_page_free(vaddr))
			return false;
		map_page_at(0, vaddr, Flags::Reserved, MemoryType::Normal, invalidate);
		return true;
	}

	bool PageTable::reserve_range(vaddr_t vaddr, size_t bytes, bool only_free)
	{
		if (size_t rem = bytes % PAGE_SIZE)
			bytes += PAGE_SIZE - rem;
		ASSERT(vaddr % PAGE_SIZE == 0);

		SpinLockGuard _(m_lock);
		if (only_free && !is_range_free(vaddr, bytes))
			return false;
		for (size_t offset = 0; offset < bytes; offset += PAGE_SIZE)
			reserve_page(vaddr + offset, true, false);
		invalidate_range(vaddr, bytes / PAGE_SIZE, true);
		return true;
	}

	vaddr_t PageTable::reserve_free_page(vaddr_t first_address, vaddr_t last_address)
	{
		if (first_address >= KERNEL_OFFSET && first_address < (vaddr_t)g_kernel_end)
			first_address = (vaddr_t)g_kernel_end;
		if (size_t rem = first_address % PAGE_SIZE)
			first_address += PAGE_SIZE - rem;
		if (size_t rem = last_address % PAGE_SIZE)
			last_address -= rem;

		ASSERT(is_canonical(first_address));
		ASSERT(is_canonical(last_address - 1));
		const vaddr_t uc_vaddr_start = uncanonicalize(first_address);
		const vaddr_t uc_vaddr_end   = uncanonicalize(last_address - 1);

		uint16_t pml4e = (uc_vaddr_start >> 39) & 0x1FF;
		uint16_t pdpte = (uc_vaddr_start >> 30) & 0x1FF;
		uint16_t pde   = (uc_vaddr_start >> 21) & 0x1FF;
		uint16_t pte   = (uc_vaddr_start >> 12) & 0x1FF;

		const uint16_t e_pml4e = (uc_vaddr_end >> 39) & 0x1FF;
		const uint16_t e_pdpte = (uc_vaddr_end >> 30) & 0x1FF;
		const uint16_t e_pde   = (uc_vaddr_end >> 21) & 0x1FF;
		const uint16_t e_pte   = (uc_vaddr_end >> 12) & 0x1FF;

		SpinLockGuard _(m_lock);

		// Try to find free page that can be mapped without
		// allocations (page table with unused entries)
		const uint64_t* pml4 = P2V(m_highest_paging_struct);
		for (; pml4e <= e_pml4e; pml4e++)
		{
			if (!(pml4[pml4e] & Flags::Present))
				continue;
			const uint64_t* pdpt = P2V(pml4[pml4e] & s_page_addr_mask);
			for (; pdpte < 512; pdpte++)
			{
				if (pml4e == e_pml4e && pdpte > e_pdpte)
					break;
				if (!(pdpt[pdpte] & Flags::Present))
					continue;
				const uint64_t* pd = P2V(pdpt[pdpte] & s_page_addr_mask);
				for (; pde < 512; pde++)
				{
					if (pml4e == e_pml4e && pdpte == e_pdpte && pde > e_pde)
						break;
					if (!(pd[pde] & Flags::Present))
						continue;
					const uint64_t* pt = P2V(pd[pde] & s_page_addr_mask);
					for (; pte < 512; pte++)
					{
						if (pml4e == e_pml4e && pdpte == e_pdpte && pde == e_pde && pte > e_pte)
							break;
						if (pt[pte] & Flags::Used)
							continue;
						vaddr_t vaddr = 0;
						vaddr |= static_cast<uint64_t>(pml4e) << 39;
						vaddr |= static_cast<uint64_t>(pdpte) << 30;
						vaddr |= static_cast<uint64_t>(pde)   << 21;
						vaddr |= static_cast<uint64_t>(pte)   << 12;
						vaddr = canonicalize(vaddr);
						ASSERT(reserve_page(vaddr));
						return vaddr;
					}
					pte = 0;
				}
				pde = 0;
			}
			pdpte = 0;
		}

		for (vaddr_t uc_vaddr = uc_vaddr_start; uc_vaddr < uc_vaddr_end; uc_vaddr += PAGE_SIZE)
		{
			if (vaddr_t vaddr = canonicalize(uc_vaddr); is_page_free(vaddr))
			{
				ASSERT(reserve_page(vaddr));
				return vaddr;
			}
		}

		return 0;
	}

	vaddr_t PageTable::reserve_free_contiguous_pages(size_t page_count, vaddr_t first_address, vaddr_t last_address)
	{
		if (first_address >= KERNEL_OFFSET && first_address < (vaddr_t)g_kernel_start)
			first_address = (vaddr_t)g_kernel_start;
		if (size_t rem = first_address % PAGE_SIZE)
			first_address += PAGE_SIZE - rem;
		if (size_t rem = last_address % PAGE_SIZE)
			last_address -= rem;

		ASSERT(is_canonical(first_address));
		ASSERT(is_canonical(last_address - 1));

		SpinLockGuard _(m_lock);

		for (vaddr_t vaddr = first_address; vaddr < last_address;)
		{
			bool valid { true };
			for (size_t page = 0; page < page_count; page++)
			{
				if (!is_canonical(vaddr + page * PAGE_SIZE))
				{
					vaddr = canonicalize(uncanonicalize(vaddr) + page * PAGE_SIZE);
					valid = false;
					break;
				}
				if (!is_page_free(vaddr + page * PAGE_SIZE))
				{
					vaddr += (page + 1) * PAGE_SIZE;
					valid = false;
					break;
				}
			}
			if (valid)
			{
				ASSERT(reserve_range(vaddr, page_count * PAGE_SIZE));
				return vaddr;
			}
		}

		return 0;
	}

	bool PageTable::is_page_free(vaddr_t page) const
	{
		ASSERT(page % PAGE_SIZE == 0);
		return !(get_page_flags(page) & Flags::Used);
	}

	bool PageTable::is_range_free(vaddr_t vaddr, size_t size) const
	{
		vaddr_t s_page = vaddr / PAGE_SIZE;
		vaddr_t e_page = BAN::Math::div_round_up<vaddr_t>(vaddr + size, PAGE_SIZE);

		SpinLockGuard _(m_lock);
		for (vaddr_t page = s_page; page < e_page; page++)
			if (!is_page_free(page * PAGE_SIZE))
				return false;
		return true;
	}

	static void dump_range(vaddr_t start, vaddr_t end, PageTable::flags_t flags)
	{
		if (start == 0)
			return;
		dprintln("{}-{}: {}{}{}{}",
			(void*)canonicalize(start),
			(void*)canonicalize(end - 1),
			flags & PageTable::Flags::Execute			? 'x' : '-',
			flags & PageTable::Flags::UserSupervisor	? 'u' : '-',
			flags & PageTable::Flags::ReadWrite			? 'w' : '-',
			flags & PageTable::Flags::Present			? 'r' : '-'
		);
	}

	void PageTable::debug_dump()
	{
		SpinLockGuard _(m_lock);

		flags_t flags = 0;
		vaddr_t start = 0;

		const uint64_t* pml4 = P2V(m_highest_paging_struct);
		for (uint64_t pml4e = 0; pml4e < 512; pml4e++)
		{
			if (!(pml4[pml4e] & Flags::Present) || (pml4e >= 256 && pml4e < 511))
			{
				dump_range(start, (pml4e << 39), flags);
				start = 0;
				continue;
			}
			const uint64_t* pdpt = P2V(pml4[pml4e] & s_page_addr_mask);
			for (uint64_t pdpte = 0; pdpte < 512; pdpte++)
			{
				if (!(pdpt[pdpte] & Flags::Present))
				{
					dump_range(start, (pml4e << 39) | (pdpte << 30), flags);
					start = 0;
					continue;
				}
				const uint64_t* pd = P2V(pdpt[pdpte] & s_page_addr_mask);
				for (uint64_t pde = 0; pde < 512; pde++)
				{
					if (!(pd[pde] & Flags::Present))
					{
						dump_range(start, (pml4e << 39) | (pdpte << 30) | (pde << 21), flags);
						start = 0;
						continue;
					}
					const uint64_t* pt = P2V(pd[pde] & s_page_addr_mask);
					for (uint64_t pte = 0; pte < 512; pte++)
					{
						if (parse_flags(pt[pte]) != flags)
						{
							dump_range(start, (pml4e << 39) | (pdpte << 30) | (pde << 21) | (pte << 12), flags);
							start = 0;
						}

						if (!(pt[pte] & Flags::Used))
							continue;

						if (start == 0)
						{
							flags = parse_flags(pt[pte]);
							start = (pml4e << 39) | (pdpte << 30) | (pde << 21) | (pte << 12);
						}
					}
				}
			}
		}
	}

}
