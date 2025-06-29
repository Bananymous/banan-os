#include <kernel/BootInfo.h>
#include <kernel/CPUID.h>
#include <kernel/Lock/SpinLock.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Memory/PageTable.h>

extern uint8_t g_kernel_start[];
extern uint8_t g_kernel_end[];

extern uint8_t g_kernel_execute_start[];
extern uint8_t g_kernel_execute_end[];

extern uint8_t g_kernel_writable_start[];
extern uint8_t g_kernel_writable_end[];

extern uint8_t g_userspace_start[];
extern uint8_t g_userspace_end[];

namespace Kernel
{

	SpinLock PageTable::s_fast_page_lock;

	static PageTable* s_kernel = nullptr;
	static bool s_has_nxe = false;
	static bool s_has_pge = false;
	static bool s_has_pat = false;

	static paddr_t s_global_pdpte = 0;

	static inline PageTable::flags_t parse_flags(uint64_t entry)
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

	void PageTable::initialize_pre_heap()
	{
		if (CPUID::has_nxe())
			s_has_nxe = true;

		if (CPUID::has_pge())
			s_has_pge = true;

		if (CPUID::has_pat())
			s_has_pat = true;

		ASSERT(s_kernel == nullptr);
		s_kernel = new PageTable();
		ASSERT(s_kernel);

		s_kernel->initialize_kernel();
		s_kernel->initial_load();
	}

	void PageTable::initialize_post_heap()
	{
		// NOTE: this is no-op as our 32 bit target does not use hhdm
	}

	void PageTable::initial_load()
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
				"movl %%cr4, %%eax;"
				"orl $0x80, %%eax;"
				"movl %%eax, %%cr4;"
				::: "eax"
			);
		}

		if (s_has_pat)
		{
			asm volatile(
				"movl $0x277, %%ecx;"
				"rdmsr;"
				"movw $0x0401, %%dx;"
				"wrmsr;"
				::: "eax", "ecx", "edx", "memory"
			);
		}

		// enable write protect
		asm volatile(
			"movl %%cr0, %%eax;"
			"orl $0x10000, %%eax;"
			"movl %%eax, %%cr0;"
			::: "rax"
		);

		load();
	}

	PageTable& PageTable::kernel()
	{
		ASSERT(s_kernel);
		return *s_kernel;
	}

	bool PageTable::is_valid_pointer(uintptr_t)
	{
		return true;
	}

	static uint64_t* allocate_zeroed_page_aligned_page()
	{
		void* page = kmalloc(PAGE_SIZE, PAGE_SIZE, true);
		ASSERT(page);
		memset(page, 0, PAGE_SIZE);
		return (uint64_t*)page;
	}

	template<typename T>
	static paddr_t V2P(const T vaddr)
	{
		return (vaddr_t)vaddr - KERNEL_OFFSET + g_boot_info.kernel_paddr;
	}

	template<typename T>
	static vaddr_t P2V(const T paddr)
	{
		return (paddr_t)paddr - g_boot_info.kernel_paddr + KERNEL_OFFSET;
	}

	void PageTable::initialize_kernel()
	{
		ASSERT(s_global_pdpte == 0);
		s_global_pdpte = V2P(allocate_zeroed_page_aligned_page());

		map_kernel_memory();

		prepare_fast_page();

		// Map (phys_kernel_start -> phys_kernel_end) to (virt_kernel_start -> virt_kernel_end)
		ASSERT((vaddr_t)g_kernel_start % PAGE_SIZE == 0);
		map_range_at(
			V2P(g_kernel_start),
			(vaddr_t)g_kernel_start,
			g_kernel_end - g_kernel_start,
			Flags::Present
		);

		// Map executable kernel memory as executable
		map_range_at(
			V2P(g_kernel_execute_start),
			(vaddr_t)g_kernel_execute_start,
			g_kernel_execute_end - g_kernel_execute_start,
			Flags::Execute | Flags::Present
		);

		// Map writable kernel memory as writable
		map_range_at(
			V2P(g_kernel_writable_start),
			(vaddr_t)g_kernel_writable_start,
			g_kernel_writable_end - g_kernel_writable_start,
			Flags::ReadWrite | Flags::Present
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
		constexpr uint64_t pdpte = (fast_page() >> 30) & 0x1FF;
		constexpr uint64_t pde   = (fast_page() >> 21) & 0x1FF;

		uint64_t* pdpt = reinterpret_cast<uint64_t*>(P2V(m_highest_paging_struct));
		ASSERT(pdpt[pdpte] & Flags::Present);

		uint64_t* pd = reinterpret_cast<uint64_t*>(P2V(pdpt[pdpte]) & PAGE_ADDR_MASK);
		ASSERT(!(pd[pde] & Flags::Present));
		pd[pde] = V2P(allocate_zeroed_page_aligned_page()) | Flags::ReadWrite | Flags::Present;
	}

	void PageTable::map_fast_page(paddr_t paddr)
	{
		ASSERT(s_kernel);
		ASSERT(paddr);
		ASSERT(paddr % PAGE_SIZE == 0);

		ASSERT(s_fast_page_lock.current_processor_has_lock());

		constexpr uint64_t pdpte = (fast_page() >> 30) & 0x1FF;
		constexpr uint64_t pde   = (fast_page() >> 21) & 0x1FF;
		constexpr uint64_t pte   = (fast_page() >> 12) & 0x1FF;

		uint64_t* pdpt = reinterpret_cast<uint64_t*>(P2V(s_kernel->m_highest_paging_struct));
		uint64_t* pd   = reinterpret_cast<uint64_t*>(P2V(pdpt[pdpte] & PAGE_ADDR_MASK));
		uint64_t* pt   = reinterpret_cast<uint64_t*>(P2V(pd[pde] & PAGE_ADDR_MASK));

		ASSERT(!(pt[pte] & Flags::Present));
		pt[pte] = paddr | Flags::ReadWrite | Flags::Present;

		invalidate(fast_page(), false);
	}

	void PageTable::unmap_fast_page()
	{
		ASSERT(s_kernel);

		ASSERT(s_fast_page_lock.current_processor_has_lock());

		constexpr uint64_t pdpte = (fast_page() >> 30) & 0x1FF;
		constexpr uint64_t pde   = (fast_page() >> 21) & 0x1FF;
		constexpr uint64_t pte   = (fast_page() >> 12) & 0x1FF;

		uint64_t* pdpt = reinterpret_cast<uint64_t*>(P2V(s_kernel->m_highest_paging_struct));
		uint64_t* pd   = reinterpret_cast<uint64_t*>(P2V(pdpt[pdpte] & PAGE_ADDR_MASK));
		uint64_t* pt   = reinterpret_cast<uint64_t*>(P2V(pd[pde] & PAGE_ADDR_MASK));

		ASSERT(pt[pte] & Flags::Present);
		pt[pte] = 0;

		invalidate(fast_page(), false);
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
		ASSERT(s_global_pdpte);

		ASSERT(m_highest_paging_struct == 0);
		m_highest_paging_struct = V2P(kmalloc(32, 32, true));
		ASSERT(m_highest_paging_struct);

		uint64_t* pdpt = reinterpret_cast<uint64_t*>(P2V(m_highest_paging_struct));
		pdpt[0] = 0;
		pdpt[1] = 0;
		pdpt[2] = 0;
		pdpt[3] = s_global_pdpte | Flags::Present;
		static_assert(KERNEL_OFFSET == 0xC0000000);
	}

	PageTable::~PageTable()
	{
		if (m_highest_paging_struct == 0)
			return;

		uint64_t* pdpt = reinterpret_cast<uint64_t*>(P2V(m_highest_paging_struct));

		for (uint32_t pdpte = 0; pdpte < 3; pdpte++)
		{
			if (!(pdpt[pdpte] & Flags::Present))
				continue;
			uint64_t* pd = reinterpret_cast<uint64_t*>(P2V(pdpt[pdpte] & PAGE_ADDR_MASK));
			for (uint32_t pde = 0; pde < 512; pde++)
			{
				if (!(pd[pde] & Flags::Present))
					continue;
				kfree(reinterpret_cast<uint64_t*>(P2V(pd[pde] & PAGE_ADDR_MASK)));
			}
			kfree(pd);
		}
		kfree(pdpt);
	}

	void PageTable::load()
	{
		SpinLockGuard _(m_lock);
		ASSERT(m_highest_paging_struct < 0x100000000);
		const uint32_t pdpt_lo = m_highest_paging_struct;
		asm volatile("movl %0, %%cr3" :: "r"(pdpt_lo));
		Processor::set_current_page_table(this);
	}

	void PageTable::invalidate(vaddr_t vaddr, bool send_smp_message)
	{
		ASSERT(vaddr % PAGE_SIZE == 0);
		asm volatile("invlpg (%0)" :: "r"(vaddr) : "memory");

		if (send_smp_message)
		{
			Processor::broadcast_smp_message({
				.type = Processor::SMPMessage::Type::FlushTLB,
				.flush_tlb = {
					.vaddr      = vaddr,
					.page_count = 1
				}
			});
		}
	}

	void PageTable::unmap_page(vaddr_t vaddr, bool send_smp_message)
	{
		ASSERT(vaddr);
		ASSERT(vaddr % PAGE_SIZE == 0);
		ASSERT(vaddr != fast_page());
		if (vaddr >= KERNEL_OFFSET)
			ASSERT(vaddr >= (vaddr_t)g_kernel_start);
		if ((vaddr >= KERNEL_OFFSET) != (this == s_kernel))
			Kernel::panic("unmapping {8H}, kernel: {}", vaddr, this == s_kernel);

		const uint64_t pdpte = (vaddr >> 30) & 0x1FF;
		const uint64_t pde   = (vaddr >> 21) & 0x1FF;
		const uint64_t pte   = (vaddr >> 12) & 0x1FF;

		SpinLockGuard _(m_lock);

		if (is_page_free(vaddr))
			Kernel::panic("trying to unmap unmapped page 0x{H}", vaddr);

		uint64_t* pdpt = reinterpret_cast<uint64_t*>(P2V(m_highest_paging_struct));
		uint64_t* pd   = reinterpret_cast<uint64_t*>(P2V(pdpt[pdpte] & PAGE_ADDR_MASK));
		uint64_t* pt   = reinterpret_cast<uint64_t*>(P2V(pd[pde] & PAGE_ADDR_MASK));

		pt[pte] = 0;
		invalidate(vaddr, send_smp_message);
	}

	void PageTable::unmap_range(vaddr_t vaddr, size_t size)
	{
		ASSERT(vaddr % PAGE_SIZE == 0);

		size_t page_count = range_page_count(vaddr, size);

		SpinLockGuard _(m_lock);
		for (vaddr_t page = 0; page < page_count; page++)
			unmap_page(vaddr + page * PAGE_SIZE, false);

		Processor::broadcast_smp_message({
			.type = Processor::SMPMessage::Type::FlushTLB,
			.flush_tlb = {
				.vaddr      = vaddr,
				.page_count = page_count
			}
		});
	}

	void PageTable::map_page_at(paddr_t paddr, vaddr_t vaddr, flags_t flags, MemoryType memory_type, bool send_smp_message)
	{
		ASSERT(vaddr);
		ASSERT(vaddr != fast_page());
		if ((vaddr >= KERNEL_OFFSET) != (this == s_kernel))
			Kernel::panic("mapping {8H} to {8H}, kernel: {}", paddr, vaddr, this == s_kernel);

		ASSERT(paddr % PAGE_SIZE == 0);
		ASSERT(vaddr % PAGE_SIZE == 0);
		ASSERT(flags & Flags::Used);

		const uint64_t pdpte = (vaddr >> 30) & 0x1FF;
		const uint64_t pde   = (vaddr >> 21) & 0x1FF;
		const uint64_t pte   = (vaddr >> 12) & 0x1FF;

		uint64_t extra_flags = 0;
		if (s_has_pge && vaddr >= KERNEL_OFFSET) // Map kernel memory as global
			extra_flags |= 1ull << 8;
		if (s_has_nxe && !(flags & Flags::Execute))
			extra_flags |= 1ull << 63;
		if (flags & Flags::Reserved)
			extra_flags |= Flags::Reserved;

		if (memory_type == MemoryType::Uncached)
			extra_flags |= (1ull << 4);
		if (s_has_pat && memory_type == MemoryType::WriteCombining)
			extra_flags |= (1ull << 7);
		if (s_has_pat && memory_type == MemoryType::WriteThrough)
			extra_flags |= (1ull << 7) | (1ull << 3);

		// NOTE: we add present here, since it has to be available in higher level structures
		flags_t uwr_flags = (flags & (Flags::UserSupervisor | Flags::ReadWrite)) | Flags::Present;

		SpinLockGuard _(m_lock);

		uint64_t* pdpt = reinterpret_cast<uint64_t*>(P2V(m_highest_paging_struct));
		if (!(pdpt[pdpte] & Flags::Present))
			pdpt[pdpte] = V2P(allocate_zeroed_page_aligned_page()) | Flags::Present;

		uint64_t* pd = reinterpret_cast<uint64_t*>(P2V(pdpt[pdpte] & PAGE_ADDR_MASK));
		if ((pd[pde] & uwr_flags) != uwr_flags)
		{
			if (!(pd[pde] & Flags::Present))
				pd[pde] = V2P(allocate_zeroed_page_aligned_page());
			pd[pde] |= uwr_flags;
		}

		if (!(flags & Flags::Present))
			uwr_flags &= ~Flags::Present;

		uint64_t* pt = reinterpret_cast<uint64_t*>(P2V(pd[pde] & PAGE_ADDR_MASK));
		pt[pte] = paddr | uwr_flags | extra_flags;

		invalidate(vaddr, send_smp_message);
	}

	void PageTable::map_range_at(paddr_t paddr, vaddr_t vaddr, size_t size, flags_t flags, MemoryType memory_type)
	{
		ASSERT(vaddr);
		ASSERT(paddr % PAGE_SIZE == 0);
		ASSERT(vaddr % PAGE_SIZE == 0);

		size_t page_count = range_page_count(vaddr, size);

		SpinLockGuard _(m_lock);
		for (size_t page = 0; page < page_count; page++)
			map_page_at(paddr + page * PAGE_SIZE, vaddr + page * PAGE_SIZE, flags, memory_type, false);

		Processor::broadcast_smp_message({
			.type = Processor::SMPMessage::Type::FlushTLB,
			.flush_tlb = {
				.vaddr      = vaddr,
				.page_count = page_count
			}
		});
	}

	uint64_t PageTable::get_page_data(vaddr_t vaddr) const
	{
		ASSERT(vaddr % PAGE_SIZE == 0);

		const uint64_t pdpte = (vaddr >> 30) & 0x1FF;
		const uint64_t pde   = (vaddr >> 21) & 0x1FF;
		const uint64_t pte   = (vaddr >> 12) & 0x1FF;

		SpinLockGuard _(m_lock);

		uint64_t* pdpt = (uint64_t*)P2V(m_highest_paging_struct);
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

	PageTable::flags_t PageTable::get_page_flags(vaddr_t vaddr) const
	{
		return parse_flags(get_page_data(vaddr));
	}

	paddr_t PageTable::physical_address_of(vaddr_t vaddr) const
	{
		uint64_t page_data = get_page_data(vaddr);
		return (page_data & PAGE_ADDR_MASK) & ~(1ull << 63);
	}

	bool PageTable::is_page_free(vaddr_t vaddr) const
	{
		ASSERT(vaddr % PAGE_SIZE == 0);
		return !(get_page_flags(vaddr) & Flags::Used);
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

		const uint32_t s_pdpte = (first_address >> 30) & 0x1FF;
		const uint32_t s_pde   = (first_address >> 21) & 0x1FF;
		const uint32_t s_pte   = (first_address >> 12) & 0x1FF;

		const uint32_t e_pdpte = (last_address >> 30) & 0x1FF;
		const uint32_t e_pde   = (last_address >> 21) & 0x1FF;
		const uint32_t e_pte   = (last_address >> 12) & 0x1FF;

		SpinLockGuard _(m_lock);

		// Try to find free page that can be mapped without
		// allocations (page table with unused entries)
		uint64_t* pdpt = reinterpret_cast<uint64_t*>(P2V(m_highest_paging_struct));
		for (uint32_t pdpte = s_pdpte; pdpte < 4; pdpte++)
		{
			if (pdpte > e_pdpte)
				break;
			if (!(pdpt[pdpte] & Flags::Present))
				continue;
			uint64_t* pd = reinterpret_cast<uint64_t*>(P2V(pdpt[pdpte] & PAGE_ADDR_MASK));
			for (uint32_t pde = s_pde; pde < 512; pde++)
			{
				if (pdpte == e_pdpte && pde > e_pde)
					break;
				if (!(pd[pde] & Flags::Present))
					continue;
				uint64_t* pt = (uint64_t*)P2V(pd[pde] & PAGE_ADDR_MASK);
				for (uint32_t pte = s_pte; pte < 512; pte++)
				{
					if (pdpte == e_pdpte && pde == e_pde && pte >= e_pte)
						break;
					if (!(pt[pte] & Flags::Used))
					{
						vaddr_t vaddr = 0;
						vaddr |= (vaddr_t)pdpte << 30;
						vaddr |= (vaddr_t)pde   << 21;
						vaddr |= (vaddr_t)pte   << 12;
						ASSERT(reserve_page(vaddr));
						return vaddr;
					}
				}
			}
		}

		// Find any free page
		for (vaddr_t vaddr = first_address; vaddr < last_address; vaddr += PAGE_SIZE)
		{
			if (is_page_free(vaddr))
			{
				ASSERT(reserve_page(vaddr));
				return vaddr;
			}
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

		SpinLockGuard _(m_lock);

		for (vaddr_t vaddr = first_address; vaddr < last_address;)
		{
			bool valid { true };
			for (size_t page = 0; page < page_count; page++)
			{
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

	static void dump_range(vaddr_t start, vaddr_t end, PageTable::flags_t flags)
	{
		if (start == 0)
			return;
		dprintln("{}-{}: {}{}{}{}",
			(void*)(start), (void*)(end - 1),
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

		uint64_t* pdpt = reinterpret_cast<uint64_t*>(P2V(m_highest_paging_struct));
		for (uint32_t pdpte = 0; pdpte < 4; pdpte++)
		{
			if (!(pdpt[pdpte] & Flags::Present))
			{
				dump_range(start, (pdpte << 30), flags);
				start = 0;
				continue;
			}
			uint64_t* pd = (uint64_t*)P2V(pdpt[pdpte] & PAGE_ADDR_MASK);
			for (uint64_t pde = 0; pde < 512; pde++)
			{
				if (!(pd[pde] & Flags::Present))
				{
					dump_range(start, (pdpte << 30) | (pde << 21), flags);
					start = 0;
					continue;
				}
				uint64_t* pt = (uint64_t*)P2V(pd[pde] & PAGE_ADDR_MASK);
				for (uint64_t pte = 0; pte < 512; pte++)
				{
					if (parse_flags(pt[pte]) != flags)
					{
						dump_range(start, (pdpte << 30) | (pde << 21) | (pte << 12), flags);
						start = 0;
					}

					if (!(pt[pte] & Flags::Used))
						continue;

					if (start == 0)
					{
						flags = parse_flags(pt[pte]);
						start = (pdpte << 30) | (pde << 21) | (pte << 12);
					}
				}
			}
		}
	}

}
