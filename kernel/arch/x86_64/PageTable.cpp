#include <kernel/Arch.h>
#include <kernel/CPUID.h>
#include <kernel/InterruptController.h>
#include <kernel/Lock/SpinLock.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Memory/PageTable.h>

extern uint8_t g_kernel_start[];
extern uint8_t g_kernel_end[];

extern uint8_t g_kernel_execute_start[];
extern uint8_t g_kernel_execute_end[];

extern uint8_t g_userspace_start[];
extern uint8_t g_userspace_end[];

namespace Kernel
{

	RecursiveSpinLock PageTable::s_fast_page_lock;

	static PageTable* s_kernel = nullptr;
	static PageTable* s_current = nullptr;
	static bool s_has_nxe = false;
	static bool s_has_pge = false;

	// PML4 entry for kernel memory
	static paddr_t s_global_pml4e = 0;

	static constexpr inline bool is_canonical(uintptr_t addr)
	{
		constexpr uintptr_t mask = 0xFFFF800000000000;
		addr &= mask;
		return addr == mask || addr == 0;
	}

	static constexpr inline uintptr_t uncanonicalize(uintptr_t addr)
	{
		if (addr & 0x0000800000000000)
			return addr & ~0xFFFF000000000000;
		return addr;
	}

	static constexpr inline uintptr_t canonicalize(uintptr_t addr)
	{
		if (addr & 0x0000800000000000)
			return addr | 0xFFFF000000000000;
		return addr;
	}

	static inline PageTable::flags_t parse_flags(uint64_t entry)
	{
		using Flags = PageTable::Flags;

		PageTable::flags_t result = 0;
		if (s_has_nxe && !(entry & (1ull << 63)))
			result |= Flags::Execute;
		if (entry & Flags::Reserved)
			result |= Flags::Reserved;
		if (entry & Flags::CacheDisable)
			result |= Flags::CacheDisable;
		if (entry & Flags::UserSupervisor)
			result |= Flags::UserSupervisor;
		if (entry & Flags::ReadWrite)
			result |= Flags::ReadWrite;
		if (entry & Flags::Present)
			result |= Flags::Present;
		return result;
	}

	void PageTable::initialize()
	{
		if (CPUID::has_nxe())
		{
			asm volatile(
				"movl $0xC0000080, %ecx;"
				"rdmsr;"
				"orl $0x800, %eax;"
				"wrmsr"
			);
			s_has_nxe = true;
		}

		uint32_t ecx, edx;
		CPUID::get_features(ecx, edx);
		if (edx & CPUID::EDX_PGE)
		{
			asm volatile(
				"movq %cr4, %rax;"
				"orq $0x80, %rax;"
				"movq %rax, %cr4;"
			);
			s_has_pge = true;
		}

		// enable write protect to kernel
		asm volatile(
			"movq %cr0, %rax;"
			"orq $0x10000, %rax;"
			"movq %rax, %cr0;"
		);

		ASSERT(s_kernel == nullptr);
		s_kernel = new PageTable();
		ASSERT(s_kernel);
		s_kernel->initialize_kernel();
		s_kernel->load();
	}

	PageTable& PageTable::kernel()
	{
		ASSERT(s_kernel);
		return *s_kernel;
	}

	PageTable& PageTable::current()
	{
		ASSERT(s_current);
		return *s_current;
	}

	bool PageTable::is_valid_pointer(uintptr_t pointer)
	{
		if (!is_canonical(pointer))
			return false;
		return true;
	}

	static uint64_t* allocate_zeroed_page_aligned_page()
	{
		void* page = kmalloc(PAGE_SIZE, PAGE_SIZE, true);
		ASSERT(page);
		memset(page, 0, PAGE_SIZE);
		return (uint64_t*)page;
	}

	void PageTable::initialize_kernel()
	{
		ASSERT(s_global_pml4e == 0);
		s_global_pml4e = V2P(allocate_zeroed_page_aligned_page());

		m_highest_paging_struct = V2P(allocate_zeroed_page_aligned_page());

		uint64_t* pml4 = (uint64_t*)P2V(m_highest_paging_struct);
		pml4[511] = s_global_pml4e;

		prepare_fast_page();

		// Map main bios area below 1 MiB
		map_range_at(
			0x000E0000,
			P2V(0x000E0000),
			0x00100000 - 0x000E0000,
			PageTable::Flags::Present
		);

		// Map (phys_kernel_start -> phys_kernel_end) to (virt_kernel_start -> virt_kernel_end)
		ASSERT((vaddr_t)g_kernel_start % PAGE_SIZE == 0);
		map_range_at(
			V2P(g_kernel_start),
			(vaddr_t)g_kernel_start,
			g_kernel_end - g_kernel_start,
			Flags::ReadWrite | Flags::Present
		);

		// Map executable kernel memory as executable
		map_range_at(
			V2P(g_kernel_execute_start),
			(vaddr_t)g_kernel_execute_start,
			g_kernel_execute_end - g_kernel_execute_start,
			Flags::Execute | Flags::Present
		);

		// Map userspace memory
		map_range_at(
			V2P(g_userspace_start),
			(vaddr_t)g_userspace_start,
			g_userspace_end - g_userspace_start,
			Flags::Execute | Flags::UserSupervisor | Flags::Present
		);
	}

	void PageTable::prepare_fast_page()
	{
		constexpr vaddr_t uc_vaddr = uncanonicalize(fast_page());
		constexpr uint64_t pml4e = (uc_vaddr >> 39) & 0x1FF;
		constexpr uint64_t pdpte = (uc_vaddr >> 30) & 0x1FF;
		constexpr uint64_t pde   = (uc_vaddr >> 21) & 0x1FF;
		constexpr uint64_t pte   = (uc_vaddr >> 12) & 0x1FF;

		uint64_t* pml4 = (uint64_t*)P2V(m_highest_paging_struct);
		ASSERT(!(pml4[pml4e] & Flags::Present));
		pml4[pml4e] = V2P(allocate_zeroed_page_aligned_page()) | Flags::ReadWrite | Flags::Present;

		uint64_t* pdpt = (uint64_t*)P2V(pml4[pml4e] & PAGE_ADDR_MASK);
		ASSERT(!(pdpt[pdpte] & Flags::Present));
		pdpt[pdpte] = V2P(allocate_zeroed_page_aligned_page()) | Flags::ReadWrite | Flags::Present;

		uint64_t* pd = (uint64_t*)P2V(pdpt[pdpte] & PAGE_ADDR_MASK);
		ASSERT(!(pd[pde] & Flags::Present));
		pd[pde] = V2P(allocate_zeroed_page_aligned_page()) | Flags::ReadWrite | Flags::Present;

		uint64_t* pt = (uint64_t*)P2V(pd[pde] & PAGE_ADDR_MASK);
		ASSERT(!(pt[pte] & Flags::Present));
		pt[pte] = V2P(allocate_zeroed_page_aligned_page());
	}

	void PageTable::map_fast_page(paddr_t paddr)
	{
		ASSERT(s_kernel);
		ASSERT_NEQ(paddr, 0);

		SpinLockGuard _(s_fast_page_lock);

		constexpr vaddr_t uc_vaddr = uncanonicalize(fast_page());
		constexpr uint64_t pml4e = (uc_vaddr >> 39) & 0x1FF;
		constexpr uint64_t pdpte = (uc_vaddr >> 30) & 0x1FF;
		constexpr uint64_t pde   = (uc_vaddr >> 21) & 0x1FF;
		constexpr uint64_t pte   = (uc_vaddr >> 12) & 0x1FF;

		uint64_t* pml4 = (uint64_t*)P2V(s_kernel->m_highest_paging_struct);
		uint64_t* pdpt = (uint64_t*)P2V(pml4[pml4e] & PAGE_ADDR_MASK);
		uint64_t* pd   = (uint64_t*)P2V(pdpt[pdpte] & PAGE_ADDR_MASK);
		uint64_t* pt   = (uint64_t*)P2V(pd[pde] & PAGE_ADDR_MASK);

		ASSERT(!(pt[pte] & Flags::Present));
		pt[pte] = paddr | Flags::ReadWrite | Flags::Present;

		invalidate(fast_page());
	}

	void PageTable::unmap_fast_page()
	{
		ASSERT(s_kernel);

		SpinLockGuard _(s_fast_page_lock);

		constexpr vaddr_t uc_vaddr = uncanonicalize(fast_page());
		constexpr uint64_t pml4e = (uc_vaddr >> 39) & 0x1FF;
		constexpr uint64_t pdpte = (uc_vaddr >> 30) & 0x1FF;
		constexpr uint64_t pde   = (uc_vaddr >> 21) & 0x1FF;
		constexpr uint64_t pte   = (uc_vaddr >> 12) & 0x1FF;

		uint64_t* pml4 = (uint64_t*)P2V(s_kernel->m_highest_paging_struct);
		uint64_t* pdpt = (uint64_t*)P2V(pml4[pml4e] & PAGE_ADDR_MASK);
		uint64_t* pd   = (uint64_t*)P2V(pdpt[pdpte] & PAGE_ADDR_MASK);
		uint64_t* pt   = (uint64_t*)P2V(pd[pde] & PAGE_ADDR_MASK);

		ASSERT(pt[pte] & Flags::Present);
		pt[pte] = 0;

		invalidate(fast_page());
	}

	BAN::ErrorOr<PageTable*> PageTable::create_userspace()
	{
		SpinLockGuard _(s_kernel->m_lock);
		PageTable* page_table = new PageTable;
		if (page_table == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		page_table->map_kernel_memory();
		return page_table;
	}

	void PageTable::map_kernel_memory()
	{
		ASSERT(s_kernel);
		ASSERT(s_global_pml4e);

		ASSERT(m_highest_paging_struct == 0);
		m_highest_paging_struct = V2P(allocate_zeroed_page_aligned_page());

		uint64_t* kernel_pml4 = (uint64_t*)P2V(s_kernel->m_highest_paging_struct);

		uint64_t* pml4 = (uint64_t*)P2V(m_highest_paging_struct);
		pml4[511] = kernel_pml4[511];
	}

	PageTable::~PageTable()
	{
		uint64_t* pml4 = (uint64_t*)P2V(m_highest_paging_struct);

		// NOTE: we only loop until 511 since the last one is the kernel memory
		for (uint64_t pml4e = 0; pml4e < 511; pml4e++)
		{
			if (!(pml4[pml4e] & Flags::Present))
				continue;
			uint64_t* pdpt = (uint64_t*)P2V(pml4[pml4e] & PAGE_ADDR_MASK);
			for (uint64_t pdpte = 0; pdpte < 512; pdpte++)
			{
				if (!(pdpt[pdpte] & Flags::Present))
					continue;
				uint64_t* pd = (uint64_t*)P2V(pdpt[pdpte] & PAGE_ADDR_MASK);
				for (uint64_t pde = 0; pde < 512; pde++)
				{
					if (!(pd[pde] & Flags::Present))
						continue;
					kfree((void*)P2V(pd[pde] & PAGE_ADDR_MASK));
				}
				kfree(pd);
			}
			kfree(pdpt);
		}
		kfree(pml4);
	}

	void PageTable::load()
	{
		SpinLockGuard _(m_lock);
		asm volatile("movq %0, %%cr3" :: "r"(m_highest_paging_struct));
		s_current = this;
	}

	void PageTable::invalidate(vaddr_t vaddr)
	{
		ASSERT(vaddr % PAGE_SIZE == 0);
		asm volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
	}

	void PageTable::unmap_page(vaddr_t vaddr)
	{
		ASSERT(vaddr);
		ASSERT(vaddr != fast_page());
		if (vaddr >= KERNEL_OFFSET)
			ASSERT_GTE(vaddr, (vaddr_t)g_kernel_start);
		if ((vaddr >= KERNEL_OFFSET) != (this == s_kernel))
			Kernel::panic("unmapping {8H}, kernel: {}", vaddr, this == s_kernel);

		ASSERT(is_canonical(vaddr));
		vaddr_t uc_vaddr = uncanonicalize(vaddr);

		ASSERT(vaddr % PAGE_SIZE == 0);

		uint64_t pml4e = (uc_vaddr >> 39) & 0x1FF;
		uint64_t pdpte = (uc_vaddr >> 30) & 0x1FF;
		uint64_t pde   = (uc_vaddr >> 21) & 0x1FF;
		uint64_t pte   = (uc_vaddr >> 12) & 0x1FF;

		SpinLockGuard _(m_lock);

		if (is_page_free(vaddr))
		{
			dwarnln("unmapping unmapped page {8H}", vaddr);
			return;
		}

		uint64_t* pml4 = (uint64_t*)P2V(m_highest_paging_struct);
		uint64_t* pdpt = (uint64_t*)P2V(pml4[pml4e] & PAGE_ADDR_MASK);
		uint64_t* pd   = (uint64_t*)P2V(pdpt[pdpte] & PAGE_ADDR_MASK);
		uint64_t* pt   = (uint64_t*)P2V(pd[pde]     & PAGE_ADDR_MASK);

		pt[pte] = 0;
		invalidate(vaddr);
	}

	void PageTable::unmap_range(vaddr_t vaddr, size_t size)
	{
		vaddr_t s_page = vaddr / PAGE_SIZE;
		vaddr_t e_page = BAN::Math::div_round_up<vaddr_t>(vaddr + size, PAGE_SIZE);

		SpinLockGuard _(m_lock);
		for (vaddr_t page = s_page; page < e_page; page++)
			unmap_page(page * PAGE_SIZE);
	}

	void PageTable::map_page_at(paddr_t paddr, vaddr_t vaddr, flags_t flags)
	{
		ASSERT(vaddr);
		ASSERT(vaddr != fast_page());
		if (vaddr >= KERNEL_OFFSET && s_current)
			ASSERT_GTE(vaddr, (vaddr_t)g_kernel_start);
		if ((vaddr >= KERNEL_OFFSET) != (this == s_kernel))
			Kernel::panic("mapping {8H} to {8H}, kernel: {}", paddr, vaddr, this == s_kernel);

		ASSERT(is_canonical(vaddr));
		vaddr_t uc_vaddr = uncanonicalize(vaddr);

		ASSERT(paddr % PAGE_SIZE == 0);
		ASSERT(vaddr % PAGE_SIZE == 0);
		ASSERT(flags & Flags::Used);

		uint64_t pml4e = (uc_vaddr >> 39) & 0x1FF;
		uint64_t pdpte = (uc_vaddr >> 30) & 0x1FF;
		uint64_t pde   = (uc_vaddr >> 21) & 0x1FF;
		uint64_t pte   = (uc_vaddr >> 12) & 0x1FF;

		uint64_t extra_flags = 0;
		if (s_has_pge && pml4e == 511) // Map kernel memory as global
			extra_flags |= 1ull << 8;
		if (s_has_nxe && !(flags & Flags::Execute))
			extra_flags |= 1ull << 63;
		if (flags & Flags::Reserved)
			extra_flags |= Flags::Reserved;
		if (flags & Flags::CacheDisable)
			extra_flags |= Flags::CacheDisable;

		// NOTE: we add present here, since it has to be available in higher level structures
		flags_t uwr_flags = (flags & (Flags::UserSupervisor | Flags::ReadWrite)) | Flags::Present;

		SpinLockGuard _(m_lock);

		uint64_t* pml4 = (uint64_t*)P2V(m_highest_paging_struct);
		if ((pml4[pml4e] & uwr_flags) != uwr_flags)
		{
			if (!(pml4[pml4e] & Flags::Present))
				pml4[pml4e] = V2P(allocate_zeroed_page_aligned_page());
			pml4[pml4e] |= uwr_flags;
		}

		uint64_t* pdpt = (uint64_t*)P2V(pml4[pml4e] & PAGE_ADDR_MASK);
		if ((pdpt[pdpte] & uwr_flags) != uwr_flags)
		{
			if (!(pdpt[pdpte] & Flags::Present))
				pdpt[pdpte] = V2P(allocate_zeroed_page_aligned_page());
			pdpt[pdpte] |= uwr_flags;
		}

		uint64_t* pd = (uint64_t*)P2V(pdpt[pdpte] & PAGE_ADDR_MASK);
		if ((pd[pde] & uwr_flags) != uwr_flags)
		{
			if (!(pd[pde] & Flags::Present))
				pd[pde] = V2P(allocate_zeroed_page_aligned_page());
			pd[pde] |= uwr_flags;
		}

		if (!(flags & Flags::Present))
			uwr_flags &= ~Flags::Present;

		uint64_t* pt = (uint64_t*)P2V(pd[pde] & PAGE_ADDR_MASK);
		pt[pte] = paddr | uwr_flags | extra_flags;

		invalidate(vaddr);
	}

	void PageTable::map_range_at(paddr_t paddr, vaddr_t vaddr, size_t size, flags_t flags)
	{
		ASSERT(is_canonical(vaddr));

		ASSERT(vaddr);
		ASSERT(paddr % PAGE_SIZE == 0);
		ASSERT(vaddr % PAGE_SIZE == 0);

		size_t page_count = range_page_count(vaddr, size);

		SpinLockGuard _(m_lock);
		for (size_t page = 0; page < page_count; page++)
			map_page_at(paddr + page * PAGE_SIZE, vaddr + page * PAGE_SIZE, flags);
	}

	uint64_t PageTable::get_page_data(vaddr_t vaddr) const
	{
		ASSERT(is_canonical(vaddr));
		vaddr_t uc_vaddr = uncanonicalize(vaddr);

		ASSERT(vaddr % PAGE_SIZE == 0);

		uint64_t pml4e = (uc_vaddr >> 39) & 0x1FF;
		uint64_t pdpte = (uc_vaddr >> 30) & 0x1FF;
		uint64_t pde   = (uc_vaddr >> 21) & 0x1FF;
		uint64_t pte   = (uc_vaddr >> 12) & 0x1FF;

		SpinLockGuard _(m_lock);

		uint64_t* pml4 = (uint64_t*)P2V(m_highest_paging_struct);
		if (!(pml4[pml4e] & Flags::Present))
			return 0;

		uint64_t* pdpt = (uint64_t*)P2V(pml4[pml4e] & PAGE_ADDR_MASK);
		if (!(pdpt[pdpte] & Flags::Present))
			return 0;

		uint64_t* pd = (uint64_t*)P2V(pdpt[pdpte] & PAGE_ADDR_MASK);
		if (!(pd[pde] & Flags::Present))
			return 0;

		uint64_t* pt = (uint64_t*)P2V(pd[pde] & PAGE_ADDR_MASK);
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
		return (page_data & PAGE_ADDR_MASK) & ~(1ull << 63);
	}

	bool PageTable::reserve_page(vaddr_t vaddr, bool only_free)
	{
		SpinLockGuard _(m_lock);
		ASSERT(vaddr % PAGE_SIZE == 0);
		if (only_free && !is_page_free(vaddr))
			return false;
		map_page_at(0, vaddr, Flags::Reserved);
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
			reserve_page(vaddr + offset);
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
		ASSERT(is_canonical(last_address));
		const vaddr_t uc_vaddr_start = uncanonicalize(first_address);
		const vaddr_t uc_vaddr_end   = uncanonicalize(last_address);

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
		uint64_t* pml4 = (uint64_t*)P2V(m_highest_paging_struct);
		for (; pml4e < 512; pml4e++)
		{
			if (pml4e > e_pml4e)
				break;
			if (!(pml4[pml4e] & Flags::Present))
				continue;
			uint64_t* pdpt = (uint64_t*)P2V(pml4[pml4e] & PAGE_ADDR_MASK);
			for (; pdpte < 512; pdpte++)
			{
				if (pml4e == e_pml4e && pdpte > e_pdpte)
					break;
				if (!(pdpt[pdpte] & Flags::Present))
					continue;
				uint64_t* pd = (uint64_t*)P2V(pdpt[pdpte] & PAGE_ADDR_MASK);
				for (; pde < 512; pde++)
				{
					if (pml4e == e_pml4e && pdpte == e_pdpte && pde > e_pde)
						break;
					if (!(pd[pde] & Flags::Present))
						continue;
					uint64_t* pt = (uint64_t*)P2V(pd[pde] & PAGE_ADDR_MASK);
					for (; pte < 512; pte++)
					{
						if (pml4e == e_pml4e && pdpte == e_pdpte && pde == e_pde && pte >= e_pte)
							break;
						if (!(pt[pte] & Flags::Used))
						{
							vaddr_t vaddr = 0;
							vaddr |= (uint64_t)pml4e << 39;
							vaddr |= (uint64_t)pdpte << 30;
							vaddr |= (uint64_t)pde   << 21;
							vaddr |= (uint64_t)pte   << 12;
							vaddr = canonicalize(vaddr);
							ASSERT(reserve_page(vaddr));
							return vaddr;
						}
					}
				}
			}
		}

		// Find any free page
		vaddr_t uc_vaddr = uc_vaddr_start;
		while (uc_vaddr < uc_vaddr_end)
		{
			if (vaddr_t vaddr = canonicalize(uc_vaddr); is_page_free(vaddr))
			{
				ASSERT(reserve_page(vaddr));
				return vaddr;
			}
			uc_vaddr += PAGE_SIZE;
		}

		ASSERT_NOT_REACHED();
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
		ASSERT(is_canonical(last_address));

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

		ASSERT_NOT_REACHED();
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

		uint64_t* pml4 = (uint64_t*)P2V(m_highest_paging_struct);
		for (uint64_t pml4e = 0; pml4e < 512; pml4e++)
		{
			if (!(pml4[pml4e] & Flags::Present))
			{
				dump_range(start, (pml4e << 39), flags);
				start = 0;
				continue;
			}
			uint64_t* pdpt = (uint64_t*)P2V(pml4[pml4e] & PAGE_ADDR_MASK);
			for (uint64_t pdpte = 0; pdpte < 512; pdpte++)
			{
				if (!(pdpt[pdpte] & Flags::Present))
				{
					dump_range(start, (pml4e << 39) | (pdpte << 30), flags);
					start = 0;
					continue;
				}
				uint64_t* pd = (uint64_t*)P2V(pdpt[pdpte] & PAGE_ADDR_MASK);
				for (uint64_t pde = 0; pde < 512; pde++)
				{
					if (!(pd[pde] & Flags::Present))
					{
						dump_range(start, (pml4e << 39) | (pdpte << 30) | (pde << 21), flags);
						start = 0;
						continue;
					}
					uint64_t* pt = (uint64_t*)P2V(pd[pde] & PAGE_ADDR_MASK);
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
