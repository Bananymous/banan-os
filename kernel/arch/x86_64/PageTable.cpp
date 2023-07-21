#include <BAN/Errors.h>
#include <kernel/Arch.h>
#include <kernel/CPUID.h>
#include <kernel/LockGuard.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Memory/PageTable.h>

extern uint8_t g_kernel_start[];
extern uint8_t g_kernel_end[];

extern uint8_t g_kernel_execute_start[];
extern uint8_t g_kernel_execute_end[];

namespace Kernel
{
	
	static PageTable* s_kernel = nullptr;
	static PageTable* s_current = nullptr;
	static bool s_has_nxe = false;
	static bool s_has_pge = false;

	// Page Directories for kernel memory (KERNEL_OFFSET -> 0xFFFFFFFFFFFFFFFF)
	static paddr_t s_global[(0xFFFFFFFFFFFFFFFF - KERNEL_OFFSET + 1) / (4096ull * 512ull * 512ull)] { };
	static_assert(sizeof(s_global) / sizeof(*s_global) < 512);

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
		return (s_has_nxe && !(entry & (1ull << 63)) ? PageTable::Flags::Execute : 0) | (entry & 0b111);
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

	static uint64_t* allocate_zeroed_page_aligned_page()
	{
		void* page = kmalloc(PAGE_SIZE, PAGE_SIZE, true);
		ASSERT(page);
		memset(page, 0, PAGE_SIZE);
		return (uint64_t*)page;
	}

	void PageTable::initialize_kernel()
	{
		for (uint32_t i = 0; i < sizeof(s_global) / sizeof(*s_global); i++)
		{
			ASSERT(s_global[i] == 0);
			s_global[i] = V2P(allocate_zeroed_page_aligned_page());
		}
		map_kernel_memory();

		// Map (0 -> phys_kernel_end) to (KERNEL_OFFSET -> virt_kernel_end)
		map_range_at(0, KERNEL_OFFSET, (uintptr_t)g_kernel_end - KERNEL_OFFSET, Flags::ReadWrite | Flags::Present);
		
		// Map executable kernel memory as executable
		map_range_at(
			V2P(g_kernel_execute_start),
			(vaddr_t)g_kernel_execute_start,
			g_kernel_execute_end - g_kernel_execute_start,
			Flags::Execute | Flags::Present
		);
	}

	BAN::ErrorOr<PageTable*> PageTable::create_userspace()
	{
		LockGuard _(s_kernel->m_lock);
		PageTable* page_table = new PageTable;
		if (page_table == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		page_table->map_kernel_memory();
		return page_table;
	}

	void PageTable::map_kernel_memory()
	{
		// Verify that kernel memory fits to single page directory pointer table
		static_assert(0xFFFFFFFFFFFFFFFF - KERNEL_OFFSET < 4096ull * 512ull * 512ull * 512ull);

		ASSERT(m_highest_paging_struct == 0);
		m_highest_paging_struct = V2P(allocate_zeroed_page_aligned_page());

		constexpr uint64_t pml4e = (KERNEL_OFFSET >> 39) & 0x1FF;
		constexpr uint64_t pdpte = (KERNEL_OFFSET >> 30) & 0x1FF;

		uint64_t* pml4 = (uint64_t*)P2V(m_highest_paging_struct);
		pml4[pml4e] = V2P(allocate_zeroed_page_aligned_page());
		pml4[pml4e] |= Flags::ReadWrite | Flags::Present;

		uint64_t* pdpt = (uint64_t*)P2V(pml4[pml4e] & PAGE_ADDR_MASK);
		for (uint64_t i = 0; pdpte + i < 512; i++)
			pdpt[pdpte + i] = s_global[i] | (Flags::ReadWrite | Flags::Present);
	}

	PageTable::~PageTable()
	{
		// Verify that kernel memory fits to single page directory pointer table
		static_assert(0xFFFFFFFFFFFFFFFF - KERNEL_OFFSET < 4096ull * 512ull * 512ull * 512ull);
		constexpr uint64_t kernel_pml4e = (KERNEL_OFFSET >> 39) & 0x1FF;
		constexpr uint64_t kernel_pdpte = (KERNEL_OFFSET >> 30) & 0x1FF;

		uint64_t* pml4 = (uint64_t*)P2V(m_highest_paging_struct);
		for (uint64_t pml4e = 0; pml4e < 512; pml4e++)
		{
			if (!(pml4[pml4e] & Flags::Present))
				continue;
			uint64_t* pdpt = (uint64_t*)P2V(pml4[pml4e] & PAGE_ADDR_MASK);
			for (uint64_t pdpte = 0; pdpte < 512; pdpte++)
			{
				if (!(pdpt[pdpte] & Flags::Present))
					continue;
				if (pml4e >= kernel_pml4e && pdpte >= kernel_pdpte)
					break;
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
		asm volatile("movq %0, %%cr3" :: "r"(m_highest_paging_struct));
		s_current = this;
	}

	void PageTable::invalidate(vaddr_t vaddr)
	{
		if (this == s_current)
			asm volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
	}

	void PageTable::unmap_page(vaddr_t vaddr)
	{
		LockGuard _(m_lock);

		vaddr &= PAGE_ADDR_MASK;

		if (vaddr && (vaddr >= KERNEL_OFFSET) != (this == s_kernel))
			Kernel::panic("unmapping {8H}, kernel: {}", vaddr, this == s_kernel);

		if (is_page_free(vaddr))
		{
			dwarnln("unmapping unmapped page {8H}", vaddr);
			return;
		}

		ASSERT(is_canonical(vaddr));
		vaddr = uncanonicalize(vaddr);

		uint64_t pml4e = (vaddr >> 39) & 0x1FF;
		uint64_t pdpte = (vaddr >> 30) & 0x1FF;
		uint64_t pde   = (vaddr >> 21) & 0x1FF;
		uint64_t pte   = (vaddr >> 12) & 0x1FF;
		
		uint64_t* pml4 = (uint64_t*)P2V(m_highest_paging_struct);
		uint64_t* pdpt = (uint64_t*)P2V(pml4[pml4e] & PAGE_ADDR_MASK);
		uint64_t* pd   = (uint64_t*)P2V(pdpt[pdpte] & PAGE_ADDR_MASK);
		uint64_t* pt   = (uint64_t*)P2V(pd[pde]     & PAGE_ADDR_MASK);

		pt[pte] = 0;
		invalidate(canonicalize(vaddr));
	}

	void PageTable::unmap_range(vaddr_t vaddr, size_t size)
	{
		LockGuard _(m_lock);

		vaddr_t s_page = vaddr / PAGE_SIZE;
		vaddr_t e_page = (vaddr + size - 1) / PAGE_SIZE;
		for (vaddr_t page = s_page; page <= e_page; page++)
			unmap_page(page * PAGE_SIZE);
	}

	void PageTable::map_page_at(paddr_t paddr, vaddr_t vaddr, flags_t flags)
	{
		LockGuard _(m_lock);

		uint64_t extra_flags = 0;
		if (s_has_nxe && !(flags & Flags::Execute))
			extra_flags |= 1ull << 63;
		if (s_has_pge && this == s_kernel)
			extra_flags |= 1ull << 8;
		flags &= ~Flags::Execute;

		if (vaddr && (vaddr >= KERNEL_OFFSET) != (this == s_kernel))
			Kernel::panic("mapping {8H} to {8H}, kernel: {}", paddr, vaddr, this == s_kernel);

		ASSERT(is_canonical(vaddr));
		vaddr = uncanonicalize(vaddr);

		ASSERT(paddr % PAGE_SIZE == 0);
		ASSERT(vaddr % PAGE_SIZE == 0);

		ASSERT(flags & Flags::Present);

		uint64_t pml4e = (vaddr >> 39) & 0x1FF;
		uint64_t pdpte = (vaddr >> 30) & 0x1FF;
		uint64_t pde   = (vaddr >> 21) & 0x1FF;
		uint64_t pte   = (vaddr >> 12) & 0x1FF;

		uint64_t* pml4 = (uint64_t*)P2V(m_highest_paging_struct);
		if ((pml4[pml4e] & flags) != flags)
		{
			if (!(pml4[pml4e] & Flags::Present))
				pml4[pml4e] = V2P(allocate_zeroed_page_aligned_page());
			pml4[pml4e] = (pml4[pml4e] & PAGE_ADDR_MASK) | flags;
		}

		uint64_t* pdpt = (uint64_t*)P2V(pml4[pml4e] & PAGE_ADDR_MASK);
		if ((pdpt[pdpte] & flags) != flags)
		{
			if (!(pdpt[pdpte] & Flags::Present))
				pdpt[pdpte] = V2P(allocate_zeroed_page_aligned_page());
			pdpt[pdpte] = (pdpt[pdpte] & PAGE_ADDR_MASK) | flags;
		}

		uint64_t* pd = (uint64_t*)P2V(pdpt[pdpte] & PAGE_ADDR_MASK);
		if ((pd[pde] & flags) != flags)
		{
			if (!(pd[pde] & Flags::Present))
				pd[pde] = V2P(allocate_zeroed_page_aligned_page());
			pd[pde] = (pd[pde] & PAGE_ADDR_MASK) | flags;
		}

		uint64_t* pt = (uint64_t*)P2V(pd[pde] & PAGE_ADDR_MASK);
		pt[pte] = paddr | flags | extra_flags;

		invalidate(canonicalize(vaddr));
	}

	void PageTable::map_range_at(paddr_t paddr, vaddr_t vaddr, size_t size, flags_t flags)
	{
		LockGuard _(m_lock);

		ASSERT(is_canonical(vaddr));

		ASSERT(paddr % PAGE_SIZE == 0);
		ASSERT(vaddr % PAGE_SIZE == 0);

		size_t first_page = vaddr / PAGE_SIZE;
		size_t last_page = (vaddr + size - 1) / PAGE_SIZE;
		size_t page_count = last_page - first_page + 1;
		for (size_t page = 0; page < page_count; page++)
			map_page_at(paddr + page * PAGE_SIZE, vaddr + page * PAGE_SIZE, flags);
	}

	uint64_t PageTable::get_page_data(vaddr_t vaddr) const
	{
		LockGuard _(m_lock);

		ASSERT(is_canonical(vaddr));
		vaddr = uncanonicalize(vaddr);

		ASSERT(vaddr % PAGE_SIZE == 0);

		uint64_t pml4e = (vaddr >> 39) & 0x1FF;
		uint64_t pdpte = (vaddr >> 30) & 0x1FF;
		uint64_t pde   = (vaddr >> 21) & 0x1FF;
		uint64_t pte   = (vaddr >> 12) & 0x1FF;
		
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
		if (!(pt[pte] & Flags::Present))
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

	vaddr_t PageTable::get_free_page(vaddr_t first_address) const
	{
		LockGuard _(m_lock);

		if (size_t rem = first_address % PAGE_SIZE)
			first_address += PAGE_SIZE - rem;

		ASSERT(is_canonical(first_address));
		vaddr_t vaddr = uncanonicalize(first_address);

		uint64_t pml4e = (vaddr >> 39) & 0x1FF;
		uint64_t pdpte = (vaddr >> 30) & 0x1FF;
		uint64_t pde   = (vaddr >> 21) & 0x1FF;
		uint64_t pte   = (vaddr >> 12) & 0x1FF;

		// Try to find free page that can be mapped without
		// allocations (page table with unused entries)
		uint64_t* pml4 = (uint64_t*)P2V(m_highest_paging_struct);
		for (; pml4e < 512; pml4e++)
		{
			if (!(pml4[pml4e] & Flags::Present))
				continue;
			uint64_t* pdpt = (uint64_t*)P2V(pml4[pml4e] & PAGE_ADDR_MASK);
			for (; pdpte < 512; pdpte++)
			{
				if (!(pdpt[pdpte] & Flags::Present))
					continue;
				uint64_t* pd = (uint64_t*)P2V(pdpt[pdpte] & PAGE_ADDR_MASK);
				for (; pde < 512; pde++)
				{
					if (!(pd[pde] & Flags::Present))
						continue;
					uint64_t* pt = (uint64_t*)P2V(pd[pde] & PAGE_ADDR_MASK);
					for (; pte < 512; pte++)
					{
						if (!(pt[pte] & Flags::Present))
						{
							vaddr_t vaddr = 0;
							vaddr |= pml4e << 39;
							vaddr |= pdpte << 30;
							vaddr |= pde   << 21;
							vaddr |= pte   << 12;
							return canonicalize(vaddr);
						}
					}
				}
			}
		}

		// Find any free page page (except for page 0)
		vaddr = first_address;
		while (is_canonical(vaddr))
		{
			if (is_page_free(vaddr))
				return vaddr;
			if (vaddr > vaddr + PAGE_SIZE)
				break;
			vaddr += PAGE_SIZE;
		}

		ASSERT_NOT_REACHED();
	}

	vaddr_t PageTable::get_free_contiguous_pages(size_t page_count, vaddr_t first_address) const
	{
		if (first_address % PAGE_SIZE)
			first_address = (first_address + PAGE_SIZE - 1) & PAGE_ADDR_MASK;

		LockGuard _(m_lock);

		for (vaddr_t vaddr = first_address; is_canonical(vaddr); vaddr += PAGE_SIZE)
		{
			bool valid { true };
			for (size_t page = 0; page < page_count; page++)
			{
				if (get_page_flags(vaddr + page * PAGE_SIZE) & Flags::Present)
				{
					vaddr += page * PAGE_SIZE;
					valid = false;
					break;
				}
			}
			if (valid)
				return vaddr;
		}

		ASSERT_NOT_REACHED();
	}

	bool PageTable::is_page_free(vaddr_t page) const
	{
		ASSERT(page % PAGE_SIZE == 0);
		return !(get_page_flags(page) & Flags::Present);
	}

	bool PageTable::is_range_free(vaddr_t start, size_t size) const
	{
		LockGuard _(m_lock);

		vaddr_t first_page = start / PAGE_SIZE;
		vaddr_t last_page = (start + size - 1) / PAGE_SIZE;
		for (vaddr_t page = first_page; page <= last_page; page++)
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
		LockGuard _(m_lock);

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

						if (!(pt[pte] & Flags::Present))
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
