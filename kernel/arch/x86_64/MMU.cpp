#include <BAN/Errors.h>
#include <kernel/kmalloc.h>
#include <kernel/MMU.h>

#define PRESENT (1 << 0)
#define READ_WRITE (1 << 1)

#define PAGE_SIZE 0x1000
#define PAGE_MASK ~(PAGE_SIZE - 1)

#define CLEANUP_STRUCTURE(s)			\
	for (uint64_t i = 0; i < 512; i++)	\
		if (s[i] & PRESENT)				\
			goto cleanup_done;			\
	kfree(s)

static MMU* s_instance = nullptr;

void MMU::Intialize()
{
	ASSERT(s_instance == nullptr);
	s_instance = new MMU();
}

MMU& MMU::Get()
{
	ASSERT(s_instance);
	return *s_instance;
}

static uint64_t* allocate_page_aligned_page()
{
	void* page = kmalloc(PAGE_SIZE, PAGE_SIZE);
	ASSERT(page);
	memset(page, 0, PAGE_SIZE);
	return (uint64_t*)page;
}

MMU::MMU()
{
	// Identity map from 4 KiB -> 4 MiB
	m_highest_paging_struct = allocate_page_aligned_page();
	
	uint64_t* pdpt = allocate_page_aligned_page();
	m_highest_paging_struct[0] = (uint64_t)pdpt | READ_WRITE | PRESENT;

	uint64_t* pd = allocate_page_aligned_page();
	pdpt[0] = (uint64_t)pd | READ_WRITE | PRESENT;

	for (uint32_t i = 0; i < 2; i++)
	{
		uint64_t* pt = allocate_page_aligned_page();
		for (uint64_t j = 0; j < 512; j++)
			pt[j] = (i << 21) | (j << 12) | READ_WRITE | PRESENT;
		pd[i] = (uint64_t)pt | READ_WRITE | PRESENT;
	}

	// Unmap 0 -> 4 KiB
	uint64_t* pt1 = (uint64_t*)(pd[0] & PAGE_MASK);
	pt1[0] = 0;

	// Load the new pml4
	asm volatile("movq %0, %%cr3" :: "r"(m_highest_paging_struct));
}

MMU::~MMU()
{
	uint64_t* pml4 = m_highest_paging_struct;
	for (uint32_t pml4e = 0; pml4e < 512; pml4e++)
	{
		if (!(pml4[pml4e] & PRESENT))
			continue;
		uint64_t* pdpt = (uint64_t*)(pml4[pml4e] & PAGE_MASK);
		for (uint32_t pdpte = 0; pdpte < 512; pdpte++)
		{
			if (!(pdpt[pdpte] & PRESENT))
				continue;
			uint64_t* pd = (uint64_t*)(pdpt[pdpte] & PAGE_MASK);
			for (uint32_t pde = 0; pde < 512; pde++)
			{
				if (!(pd[pde] & PRESENT))
					continue;
				kfree((void*)(pd[pde] & PAGE_MASK));
			}
			kfree(pd);
		}
		kfree(pdpt);
	}
	kfree(pml4);
}

void MMU::AllocatePage(uintptr_t address)
{
	ASSERT((address >> 48) == 0);

	address &= PAGE_MASK;

	uint64_t pml4e = (address >> 39) & 0x1FF;
	uint64_t pdpte = (address >> 30) & 0x1FF;
	uint64_t pde   = (address >> 21) & 0x1FF;
	uint64_t pte   = (address >> 12) & 0x1FF;

	uint64_t* pml4 = m_highest_paging_struct;
	if (!(pml4[pml4e] & PRESENT))
	{
		uint64_t* pdpt = allocate_page_aligned_page();
		pml4[pml4e] = (uint64_t)pdpt | READ_WRITE | PRESENT;
	}

	uint64_t* pdpt = (uint64_t*)(pml4[pml4e] & PAGE_MASK);
	if (!(pdpt[pdpte] & PRESENT))
	{
		uint64_t* pd = allocate_page_aligned_page();
		pdpt[pdpte] = (uint64_t)pd | READ_WRITE | PRESENT;
	}

	uint64_t* pd = (uint64_t*)(pdpt[pdpte] & PAGE_MASK);
	if (!(pd[pde] & PRESENT))
	{
		uint64_t* pt = allocate_page_aligned_page();
		pd[pde] = (uint64_t)pt | READ_WRITE | PRESENT;
	}

	uint64_t* pt = (uint64_t*)(pd[pde] & PAGE_MASK);
	if (!(pt[pte] & PRESENT))
	{
		pt[pte] = address | READ_WRITE | PRESENT;
		asm volatile("invlpg (%0)" :: "r"(address) : "memory");
	}
}

void MMU::AllocateRange(uintptr_t address, ptrdiff_t size)
{
	uintptr_t s_page = address & PAGE_MASK;
	uintptr_t e_page = (address + size - 1) & PAGE_MASK;
	for (uintptr_t page = s_page; page <= e_page; page += PAGE_SIZE)
		AllocatePage(page);
}

void MMU::UnAllocatePage(uintptr_t address)
{
	ASSERT((address >> 48) == 0);

	address &= PAGE_MASK;

	uint64_t pml4e = (address >> 39) & 0x1FF;
	uint64_t pdpte = (address >> 30) & 0x1FF;
	uint64_t pde   = (address >> 21) & 0x1FF;
	uint64_t pte   = (address >> 12) & 0x1FF;
	
	uint64_t* pml4 = m_highest_paging_struct;
	if (!(pml4[pml4e] & PRESENT))
		return;

	uint64_t* pdpt = (uint64_t*)(pml4[pml4e] & PAGE_MASK);
	if (!(pdpt[pdpte] & PRESENT))
		return;

	uint64_t* pd = (uint64_t*)(pdpt[pdpte] & PAGE_MASK);
	if (!(pd[pde] & PRESENT))
		return;

	uint64_t* pt = (uint64_t*)(pd[pde] & PAGE_MASK);
	if (!(pt[pte] & PRESENT))
		return;

	pt[pte] = 0;

	CLEANUP_STRUCTURE(pt);
	pd[pde] = 0;
	CLEANUP_STRUCTURE(pd);
	pdpt[pdpte] = 0;
	CLEANUP_STRUCTURE(pdpt);
	pml4[pml4e] = 0;
cleanup_done:
	
	asm volatile("invlpg (%0)" :: "r"(address) : "memory");
}

void MMU::UnAllocateRange(uintptr_t address, ptrdiff_t size)
{
	uintptr_t s_page = address & PAGE_MASK;
	uintptr_t e_page = (address + size - 1) & PAGE_MASK;
	for (uintptr_t page = s_page; page <= e_page; page += PAGE_SIZE)
		UnAllocatePage(page);
}
