#include <kernel/CPUID.h>
#include <kernel/Paging.h>
#include <kernel/Panic.h>

#include <string.h>

#define PRESENT		(1 << 0)
#define READ_WRITE	(1 << 1)
#define PAGE_SIZE	(1 << 7)

namespace Paging
{

	static uint64_t s_page_directory_pointer_table[4]	__attribute__((aligned(0x20)));
	static uint64_t s_page_directory[4 * 512]			__attribute__((aligned(4096)));

	static bool HasRequirements()
	{
		uint32_t ecx, edx;
		CPUID::GetFeatures(ecx, edx);
		if (!(edx & CPUID::Features::EDX_PAE))
		{
			derrorln("PAE not supported, halting");
			return false;
		}

		return true;
	}

	void Initialize()
	{
		if (!HasRequirements())
			asm volatile("hlt");

		// Disable paging
		asm volatile(
			"movl %cr0, %ebx;"
			"andl $0x7fffffff, %ebx;"
			"movl %ebx, %cr0;"
		);

		// Identity map first 2 (2 MiB) pages
		memset(s_page_directory, 0x00, sizeof(s_page_directory));
		s_page_directory[0] = (0x00 << 21) | PAGE_SIZE | READ_WRITE | PRESENT;
		s_page_directory[1] = (0x01 << 21) | PAGE_SIZE | READ_WRITE | PRESENT;

		// Initialize PDPTEs
		for (int i = 0; i < 4; i++)
			s_page_directory_pointer_table[i] = (uint64_t)(&s_page_directory[512 * i]) | PRESENT;

		asm volatile(
			// Enable PAE
			"movl %%cr4, %%eax;"
			"orl $0x20, %%eax;"
			"movl %%eax, %%cr4;"

			// Load PDPT address to cr3
			"movl %0, %%cr3;"
			
			// Enable paging
			"movl %%cr0, %%eax;"
			"orl $0x80000000, %%eax;"
			"movl %%eax, %%cr0;"

			:: "r" (s_page_directory_pointer_table)
			: "eax"
		);
	}

	void MapPage(uintptr_t address)
	{
		if (address != ((address >> 21) << 21))
		{
			dprintln("aligning 0x{8H} to 2 MiB boundary -> 0x{8H}", address, (address >> 21) << 21);
			address = (address >> 21) << 21;
		}

		uint32_t pde = address >> 21;

		dprintln("mapping pde {} (0x{8H} - 0x{8H})", pde, pde << 21, ((pde + 1) << 21) - 1);

		if (s_page_directory[pde] & PRESENT)
		{
			dprintln("page already mapped");
			return;
		}

		// Map and flush the given address
		s_page_directory[pde] = address | PAGE_SIZE | READ_WRITE | PRESENT;
		asm volatile("invlpg (%0)" :: "r"(address) : "memory");
	}

	void MapPages(uintptr_t address, size_t size)
	{
		address = (address >> 21) << 21;
		for (size_t offset = 0; offset < size; offset += 1 << 21)
			MapPage(address + offset);
	}

}