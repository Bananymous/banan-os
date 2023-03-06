#include <BAN/Errors.h>
#include <kernel/Debug.h>
#include <kernel/MMU.h>
#include <kernel/kmalloc.h>

#include <string.h>

#define MMU_DEBUG_PRINT 0

// bits 31-12 set
#define PAGE_MASK 0xfffff000
#define PAGE_SIZE 0x00001000

static MMU* s_instance = nullptr;

void MMU::intialize()
{
	ASSERT(s_instance == nullptr);
	s_instance = new MMU();
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

MMU::MMU()
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

	// reload this new pdpt
	asm volatile("movl %0, %%cr3" :: "r"(m_highest_paging_struct));
}

void MMU::allocate_page(uintptr_t address, uint8_t flags)
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

	asm volatile("invlpg (%0)" :: "r"(address) : "memory");
}

void MMU::allocate_range(uintptr_t address, ptrdiff_t size, uint8_t flags)
{
	uintptr_t s_page = address & PAGE_MASK;
	uintptr_t e_page = (address + size - 1) & PAGE_MASK;
	for (uintptr_t page = s_page; page <= e_page; page += PAGE_SIZE)
		allocate_page(page, flags);
}

void MMU::unallocate_page(uintptr_t address)
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

	asm volatile("invlpg (%0)" :: "r"(address & PAGE_MASK) : "memory");
}

void MMU::unallocate_range(uintptr_t address, ptrdiff_t size)
{
	uintptr_t s_page = address & PAGE_MASK;
	uintptr_t e_page = (address + size - 1) & PAGE_MASK;
	for (uintptr_t page = s_page; page <= e_page; page += PAGE_SIZE)
		unallocate_page(page);
}
