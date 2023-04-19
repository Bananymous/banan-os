#include <BAN/Errors.h>
#include <kernel/Debug.h>
#include <kernel/Memory/MMU.h>
#include <kernel/Memory/kmalloc.h>

#include <string.h>

#define MMU_DEBUG_PRINT 0

// bits 31-12 set
#define PAGE_MASK 0xfffff000
#define PAGE_SIZE 0x00001000

static MMU* s_instance = nullptr;

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

static uint64_t* allocate_page_aligned_page()
{
	uint64_t* page = (uint64_t*)kmalloc(PAGE_SIZE, PAGE_SIZE);
	ASSERT(page);
	ASSERT(((uintptr_t)page % PAGE_SIZE) == 0);
	memset(page, 0, PAGE_SIZE);
	return page;
}

void MMU::initialize_kernel()
{
	m_highest_paging_struct = (uint64_t*)kmalloc(sizeof(uint64_t) * 4, 32);
	ASSERT(m_highest_paging_struct);
	ASSERT(((uintptr_t)m_highest_paging_struct % 32) == 0);

	// allocate all page directories
	for (int i = 0; i < 4; i++)
	{
		uint64_t* page_directory = allocate_page_aligned_page();
		m_highest_paging_struct[i] = (uint64_t)page_directory | Flags::Present;
	}

	// FIXME: We should just identity map until g_kernel_end

	// create and identity map first 6 MiB
	uint64_t* page_directory1 = (uint64_t*)(m_highest_paging_struct[0] & PAGE_MASK);
	for (uint64_t i = 0; i < 3; i++)
	{
		uint64_t* page_table = allocate_page_aligned_page();
		for (uint64_t j = 0; j < 512; j++)
			page_table[j] = (i << 21) | (j << 12) | Flags::ReadWrite | Flags::Present;

		page_directory1[i] = (uint64_t)page_table | Flags::ReadWrite | Flags::Present;
	}

	// dont map first page (0 -> 4 KiB) so that nullptr dereference
	// causes page fault :)
	uint64_t* page_table1 = (uint64_t*)(page_directory1[0] & PAGE_MASK);
	page_table1[0] = 0;
}

MMU::MMU()
{
	if (s_instance == nullptr)
		return;
	
	// Here we copy the s_instances paging structs since they are
	// global for every process

	uint64_t* global_pdpt = s_instance->m_highest_paging_struct;

	uint64_t* pdpt = (uint64_t*)kmalloc(sizeof(uint64_t) * 4, 32);
	ASSERT(pdpt);
	
	for (uint32_t pdpte = 0; pdpte < 4; pdpte++)
	{
		if (!(global_pdpt[pdpte] & Flags::Present))
			continue;

		uint64_t* global_pd = (uint64_t*)(global_pdpt[pdpte] & PAGE_MASK);

		uint64_t* pd = allocate_page_aligned_page();
		pdpt[pdpte] = (uint64_t)pd | (global_pdpt[pdpte] & ~PAGE_MASK);

		for (uint32_t pde = 0; pde < 512; pde++)
		{
			if (!(global_pd[pde] & Flags::Present))
				continue;

			uint64_t* global_pt = (uint64_t*)(global_pd[pde] & PAGE_MASK);

			uint64_t* pt = allocate_page_aligned_page();
			pd[pde] = (uint64_t)pt | (global_pd[pde] & ~PAGE_MASK);

			memcpy(pt, global_pt, PAGE_SIZE);
		}
	}

	m_highest_paging_struct = pdpt;
}

void MMU::load()
{
	asm volatile("movl %0, %%cr3" :: "r"(m_highest_paging_struct));
}

void MMU::map_page(uintptr_t address, uint8_t flags)
{
#if MMU_DEBUG_PRINT
	dprintln("AllocatePage(0x{8H})", address);
#endif
	ASSERT(flags & Flags::Present);

	address &= PAGE_MASK;

	uint32_t pdpte = (address & 0xC0000000) >> 30;
	uint32_t pde   = (address & 0x3FE00000) >> 21;
	uint32_t pte   = (address & 0x001FF000) >> 12;

	uint64_t* page_directory = (uint64_t*)(m_highest_paging_struct[pdpte] & PAGE_MASK);
	if (!(page_directory[pde] & Flags::Present))
	{
		uint64_t* page_table = allocate_page_aligned_page();
		page_directory[pde] = (uint64_t)page_table;
	}
	page_directory[pde] |= flags;

	uint64_t* page_table = (uint64_t*)(page_directory[pde] & PAGE_MASK);
	page_table[pte] = address | flags;
}

void MMU::map_range(uintptr_t address, ptrdiff_t size, uint8_t flags)
{
	uintptr_t s_page = address & PAGE_MASK;
	uintptr_t e_page = (address + size - 1) & PAGE_MASK;
	for (uintptr_t page = s_page; page <= e_page; page += PAGE_SIZE)
		map_page(page, flags);
}

void MMU::unmap_page(uintptr_t address)
{
#if MMU_DEBUG_PRINT
	dprintln("UnAllocatePage(0x{8H})", address & PAGE_MASK);
#endif

	uint32_t pdpte = (address & 0xC0000000) >> 30;
	uint32_t pde   = (address & 0x3FE00000) >> 21;
	uint32_t pte   = (address & 0x001FF000) >> 12;

	uint64_t* page_directory = (uint64_t*)(m_highest_paging_struct[pdpte] & PAGE_MASK);
	if (!(page_directory[pde] & Flags::Present))
		return;

	uint64_t* page_table = (uint64_t*)(page_directory[pde] & PAGE_MASK);
	if (!(page_table[pte] & Flags::Present))
		return;

	page_table[pte] = 0;

	// TODO: Unallocate the page table if this was the only allocated page
}

void MMU::unmap_range(uintptr_t address, ptrdiff_t size)
{
	uintptr_t s_page = address & PAGE_MASK;
	uintptr_t e_page = (address + size - 1) & PAGE_MASK;
	for (uintptr_t page = s_page; page <= e_page; page += PAGE_SIZE)
		unmap_page(page);
}
