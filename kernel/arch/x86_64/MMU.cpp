#include <BAN/Errors.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Memory/MMU.h>

#define FLAGS_MASK (PAGE_SIZE - 1)
#define PAGE_MASK (~FLAGS_MASK)

#define CLEANUP_STRUCTURE(s)			\
	for (uint64_t i = 0; i < 512; i++)	\
		if (s[i] & Flags::Present)		\
			return;						\
	kfree(s)


extern uint8_t g_kernel_end[];

namespace Kernel
{
	
	static MMU* s_instance = nullptr;
	static MMU* s_current = nullptr;

	void MMU::initialize()
	{
		ASSERT(s_instance == nullptr);
		s_instance = new MMU();
		ASSERT(s_instance);
		s_instance->initialize_kernel();
		s_instance->load();
	}

	MMU& MMU::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	MMU& MMU::current()
	{
		ASSERT(s_current);
		return *s_current;
	}

	static uint64_t* allocate_page_aligned_page()
	{
		void* page = kmalloc(PAGE_SIZE, PAGE_SIZE);
		ASSERT(page);
		memset(page, 0, PAGE_SIZE);
		return (uint64_t*)page;
	}

	void MMU::initialize_kernel()
	{
		m_highest_paging_struct = allocate_page_aligned_page();
		memset(m_highest_paging_struct, 0, PAGE_SIZE);

		// Identity map 4 KiB -> kernel end. We don't map the first page since nullptr derefs should
		// page fault. Also there isn't anything useful in that memory.
		identity_map_range(PAGE_SIZE, (uintptr_t)g_kernel_end, Flags::ReadWrite | Flags::Present);
	}

	MMU::MMU()
	{
		if (s_instance == nullptr)
			return;
		
		// Here we copy the s_instances paging structs since they are
		// global for every process

		uint64_t* global_pml4 = s_instance->m_highest_paging_struct;

		uint64_t* pml4 = allocate_page_aligned_page();
		for (uint32_t pml4e = 0; pml4e < 512; pml4e++)
		{
			if (!(global_pml4[pml4e] & Flags::Present))
				continue;

			uint64_t* global_pdpt = (uint64_t*)(global_pml4[pml4e] & PAGE_MASK);

			uint64_t* pdpt = allocate_page_aligned_page();
			pml4[pml4e] = (uint64_t)pdpt | (global_pml4[pml4e] & FLAGS_MASK);

			for (uint32_t pdpte = 0; pdpte < 512; pdpte++)
			{
				if (!(global_pdpt[pdpte] & Flags::Present))
					continue;

				uint64_t* global_pd = (uint64_t*)(global_pdpt[pdpte] & PAGE_MASK);

				uint64_t* pd = allocate_page_aligned_page();
				pdpt[pdpte] = (uint64_t)pd | (global_pdpt[pdpte] & FLAGS_MASK);

				for (uint32_t pde = 0; pde < 512; pde++)
				{
					if (!(global_pd[pde] & Flags::Present))
						continue;

					uint64_t* global_pt = (uint64_t*)(global_pd[pde] & PAGE_MASK);

					uint64_t* pt = allocate_page_aligned_page();
					pd[pde] = (uint64_t)pt | (global_pd[pde] & FLAGS_MASK);

					memcpy(pt, global_pt, PAGE_SIZE);
				}
			}
		}

		m_highest_paging_struct = pml4;
	}

	MMU::~MMU()
	{
		uint64_t* pml4 = m_highest_paging_struct;
		for (uint32_t pml4e = 0; pml4e < 512; pml4e++)
		{
			if (!(pml4[pml4e] & Flags::Present))
				continue;
			uint64_t* pdpt = (uint64_t*)(pml4[pml4e] & PAGE_MASK);
			for (uint32_t pdpte = 0; pdpte < 512; pdpte++)
			{
				if (!(pdpt[pdpte] & Flags::Present))
					continue;
				uint64_t* pd = (uint64_t*)(pdpt[pdpte] & PAGE_MASK);
				for (uint32_t pde = 0; pde < 512; pde++)
				{
					if (!(pd[pde] & Flags::Present))
						continue;
					kfree((void*)(pd[pde] & PAGE_MASK));
				}
				kfree(pd);
			}
			kfree(pdpt);
		}
		kfree(pml4);
	}

	void MMU::load()
	{
		asm volatile("movq %0, %%cr3" :: "r"(m_highest_paging_struct));
		s_current = this;
	}

	void MMU::identity_map_page(paddr_t address, flags_t flags)
	{
		address &= PAGE_MASK;
		map_page_at(address, address, flags);
	}

	void MMU::identity_map_range(paddr_t address, size_t size, flags_t flags)
	{
		paddr_t s_page = address & PAGE_MASK;
		paddr_t e_page = (address + size - 1) & PAGE_MASK;
		for (paddr_t page = s_page; page <= e_page; page += PAGE_SIZE)
			identity_map_page(page, flags);
	}

	void MMU::unmap_page(vaddr_t address)
	{
		ASSERT((address >> 48) == 0);

		address &= PAGE_MASK;

		uint64_t pml4e = (address >> 39) & 0x1FF;
		uint64_t pdpte = (address >> 30) & 0x1FF;
		uint64_t pde   = (address >> 21) & 0x1FF;
		uint64_t pte   = (address >> 12) & 0x1FF;
		
		uint64_t* pml4 = m_highest_paging_struct;
		if (!(pml4[pml4e] & Flags::Present))
			return;

		uint64_t* pdpt = (uint64_t*)(pml4[pml4e] & PAGE_MASK);
		if (!(pdpt[pdpte] & Flags::Present))
			return;

		uint64_t* pd = (uint64_t*)(pdpt[pdpte] & PAGE_MASK);
		if (!(pd[pde] & Flags::Present))
			return;

		uint64_t* pt = (uint64_t*)(pd[pde] & PAGE_MASK);
		if (!(pt[pte] & Flags::Present))
			return;

		pt[pte] = 0;

		CLEANUP_STRUCTURE(pt);
		pd[pde] = 0;
		CLEANUP_STRUCTURE(pd);
		pdpt[pdpte] = 0;
		CLEANUP_STRUCTURE(pdpt);
		pml4[pml4e] = 0;
	}

	void MMU::unmap_range(vaddr_t address, size_t size)
	{
		vaddr_t s_page = address & PAGE_MASK;
		vaddr_t e_page = (address + size - 1) & PAGE_MASK;
		for (vaddr_t page = s_page; page <= e_page; page += PAGE_SIZE)
			unmap_page(page);
	}

	void MMU::map_page_at(paddr_t paddr, vaddr_t vaddr, flags_t flags)
	{
		ASSERT((paddr >> 48) == 0);
		ASSERT((vaddr >> 48) == 0);

		ASSERT(paddr % PAGE_SIZE == 0);
		ASSERT(vaddr % PAGE_SIZE == 0);;

		ASSERT(flags & Flags::Present);

		uint64_t pml4e = (vaddr >> 39) & 0x1FF;
		uint64_t pdpte = (vaddr >> 30) & 0x1FF;
		uint64_t pde   = (vaddr >> 21) & 0x1FF;
		uint64_t pte   = (vaddr >> 12) & 0x1FF;

		uint64_t* pml4 = m_highest_paging_struct;
		if ((pml4[pml4e] & flags) != flags)
		{
			if (!(pml4[pml4e] & Flags::Present))
				pml4[pml4e] = (uint64_t)allocate_page_aligned_page();
			pml4[pml4e] = (pml4[pml4e] & PAGE_MASK) | flags;
		}

		uint64_t* pdpt = (uint64_t*)(pml4[pml4e] & PAGE_MASK);
		if ((pdpt[pdpte] & flags) != flags)
		{
			if (!(pdpt[pdpte] & Flags::Present))
				pdpt[pdpte] = (uint64_t)allocate_page_aligned_page();
			pdpt[pdpte] = (pdpt[pdpte] & PAGE_MASK) | flags;
		}

		uint64_t* pd = (uint64_t*)(pdpt[pdpte] & PAGE_MASK);
		if ((pd[pde] & flags) != flags)
		{
			if (!(pd[pde] & Flags::Present))
				pd[pde] = (uint64_t)allocate_page_aligned_page();
			pd[pde] = (pd[pde] & PAGE_MASK) | flags;
		}

		uint64_t* pt = (uint64_t*)(pd[pde] & PAGE_MASK);
		if ((pt[pte] & flags) != flags)
			pt[pte] = paddr | flags;
	}

	uint64_t MMU::get_page_data(vaddr_t address) const
	{
		ASSERT((address >> 48) == 0);
		ASSERT(address % PAGE_SIZE == 0);

		uint64_t pml4e = (address >> 39) & 0x1FF;
		uint64_t pdpte = (address >> 30) & 0x1FF;
		uint64_t pde   = (address >> 21) & 0x1FF;
		uint64_t pte   = (address >> 12) & 0x1FF;
		
		uint64_t* pml4 = m_highest_paging_struct;
		if (!(pml4[pml4e] & Flags::Present))
			return 0;

		uint64_t* pdpt = (uint64_t*)(pml4[pml4e] & PAGE_MASK);
		if (!(pdpt[pdpte] & Flags::Present))
			return 0;

		uint64_t* pd = (uint64_t*)(pdpt[pdpte] & PAGE_MASK);
		if (!(pd[pde] & Flags::Present))
			return 0;

		uint64_t* pt = (uint64_t*)(pd[pde] & PAGE_MASK);
		if (!(pt[pte] & Flags::Present))
			return 0;

		return pt[pte];
	}

	MMU::flags_t MMU::get_page_flags(vaddr_t addr) const
	{
		return get_page_data(addr) & FLAGS_MASK;
	}

	paddr_t MMU::physical_address_of(vaddr_t addr) const
	{
		return get_page_data(addr) & PAGE_MASK;
	}

	vaddr_t MMU::get_free_page() const
	{
		// Try to find free page that can be mapped without
		// allocations (page table with unused entries)
		vaddr_t* pml4 = m_highest_paging_struct;
		for (uint64_t pml4e = 0; pml4e < 512; pml4e++)
		{
			if (!(pml4[pml4e] & Flags::Present))
				continue;
			vaddr_t* pdpt = (vaddr_t*)(pml4[pml4e] & PAGE_MASK);
			for (uint64_t pdpte = 0; pdpte < 512; pdpte++)
			{
				if (!(pdpt[pdpte] & Flags::Present))
					continue;
				vaddr_t* pd = (vaddr_t*)(pdpt[pdpte] & PAGE_MASK);
				for (uint64_t pde = 0; pde < 512; pde++)
				{
					if (!(pd[pde] & Flags::Present))
						continue;
					vaddr_t* pt = (vaddr_t*)(pd[pde] & PAGE_MASK);
					for (uint64_t pte = !(pml4e + pdpte + pde); pte < 512; pte++)
					{
						if (!(pt[pte] & Flags::Present))
						{
							vaddr_t vaddr = 0;
							vaddr |= pml4e << 39;
							vaddr |= pdpte << 30;
							vaddr |= pde   << 21;
							vaddr |= pte   << 12;
							return vaddr;
						}
					}
				}
			}
		}

		// Find any free page page (except for page 0)
		vaddr_t address = PAGE_SIZE;
		while ((address >> 48) == 0)
		{
			if (!(get_page_flags(address) & Flags::Present))
				return address;
			address += PAGE_SIZE;
		}

		ASSERT_NOT_REACHED();
	}

	vaddr_t MMU::get_free_contiguous_pages(size_t page_count) const
	{
		for (vaddr_t address = PAGE_SIZE; !(address >> 48); address += PAGE_SIZE)
		{
			bool valid { true };
			for (size_t page = 0; page < page_count; page++)
			{
				if (get_page_flags(address + page * PAGE_SIZE) & Flags::Present)
				{
					address += page;
					valid = false;
					break;
				}
			}
			if (valid)
				return address;
		}

		ASSERT_NOT_REACHED();
	}

	bool MMU::is_page_free(vaddr_t page) const
	{
		ASSERT(page % PAGE_SIZE == 0);
		return !(get_page_flags(page) & Flags::Present);
	}

	bool MMU::is_range_free(vaddr_t start, size_t size) const
	{
		vaddr_t first_page = start / PAGE_SIZE;
		vaddr_t last_page = BAN::Math::div_round_up<vaddr_t>(start + size, PAGE_SIZE);
		for (vaddr_t page = first_page; page <= last_page; page++)
			if (!is_page_free(page * PAGE_SIZE))
				return false;
		return true;
	}

}
