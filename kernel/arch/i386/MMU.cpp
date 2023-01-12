#include <BAN/Errors.h>
#include <kernel/MMU.h>
#include <kernel/kmalloc.h>
#include <kernel/Serial.h>

#include <string.h>

#define PRESENT (1 << 0)
#define READ_WRITE (1 << 1)

// bits 31-12 set
#define PAGE_MASK 0xfffff000
#define PAGE_SIZE 0x00001000

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
	uint64_t* page_directory = (uint64_t*)kmalloc(PAGE_SIZE, PAGE_SIZE);
	ASSERT(page_directory);
	ASSERT(((uintptr_t)page_directory % PAGE_SIZE) == 0);
	memset(page_directory, 0, PAGE_SIZE);
	return page_directory;
}

MMU::MMU()
{
	m_page_descriptor_pointer_table = (uint64_t*)kmalloc(sizeof(uint64_t) * 4, 32);
	ASSERT(m_page_descriptor_pointer_table);
	ASSERT(((uintptr_t)m_page_descriptor_pointer_table % 32) == 0);

	// create and zero out all page directories
	for (int i = 0; i < 4; i++)
	{
		uint64_t* page_directory = allocate_page_aligned_page();
		m_page_descriptor_pointer_table[i] = (uint64_t)page_directory | PRESENT;
	}

	// create and identity map first 4 MiB
	uint64_t* page_directory1 = (uint64_t*)(m_page_descriptor_pointer_table[0] & PAGE_MASK);
	for (uint64_t i = 0; i < 2; i++)
	{
		uint64_t* page_table = allocate_page_aligned_page();
		for (uint64_t j = 0; j < 512; j++)
			page_table[j] = (i << 21) | (j << 12) | READ_WRITE | PRESENT;

		page_directory1[i] = (uint64_t)page_table | READ_WRITE | PRESENT;
	}

	// dont map first page (0 -> 4 KiB) so that nullptr dereference
	// causes page fault :)
	uint64_t* page_table1 = (uint64_t*)page_directory1[0];
	page_table1[0] = 0;

	// reload this new pdpt
	asm volatile("movl %0, %%cr3" :: "r"(m_page_descriptor_pointer_table));
}

void MMU::AllocatePage(uintptr_t address)
{
	uint32_t pdpte = (address & 0xC0000000) >> 30;
	uint32_t pde   = (address & 0x3FE00000) >> 21;
	uint32_t pte   = (address & 0x001FF000) >> 12;

	ASSERT(pdpte < 4);

	uint64_t* page_directory = (uint64_t*)(m_page_descriptor_pointer_table[pdpte] & PAGE_MASK);
	if (!(page_directory[pde] & PRESENT))
	{
		uint64_t* page_table = allocate_page_aligned_page();
		page_directory[pde] = (uint64_t)page_table | READ_WRITE | PRESENT;
	}

	uint64_t* page_table = (uint64_t*)(page_directory[pde] & PAGE_MASK);
	page_table[pte] = (address & PAGE_MASK) | READ_WRITE | PRESENT;

	asm volatile("invlpg (%0)" :: "r"(address & PAGE_MASK) : "memory");
}

void MMU::AllocateRange(uintptr_t address, ptrdiff_t size)
{
	uintptr_t s_page = address & PAGE_MASK;
	uintptr_t e_page = (address + size - 1) & PAGE_MASK;
	for (uintptr_t page = s_page; page <= e_page; page += PAGE_SIZE)
		AllocatePage(page);
}
