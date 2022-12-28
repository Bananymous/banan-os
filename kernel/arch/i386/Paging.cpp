#include <kernel/Paging.h>
#include <kernel/panic.h>

namespace Paging
{

	static uint32_t s_page_directory[1024] __attribute__((aligned(4096)));
	static uint32_t s_page_table[1024] __attribute__((aligned(4096)));
	static uint32_t s_page_table_framebuffer[1024] __attribute__((aligned(4096)));
	static uint32_t s_page_table_rsdp[1024] __attribute__((aligned(4096)));
	static uint32_t s_page_table_apic[1024] __attribute__((aligned(4096)));

	void Initialize()
	{
		for (uint32_t i = 0; i < 1024; i++)
			s_page_directory[i] = 0x00000002;

		for (uint32_t i = 0; i < 1024; i++)
			s_page_table[i] = (i << 12) | 0x03;
		s_page_directory[0] = (uint32_t)s_page_table | 0x03;

		asm volatile(
			"movl %%eax, %%cr3;"
			"movl %%cr0, %%eax;"
			"orl $0x80000001, %%eax;"
			"movl %%eax, %%cr0;"
			:: "a"(s_page_directory)
		);
	}

	static void MapPDE(uint32_t address, uint32_t* pt)
	{
		if ((address & 0xffc00000) != address)
			Kernel::panic("Trying to map non 4 MiB aligned address");

		uint32_t pd_index = address >> 22;

		if (!(s_page_directory[pd_index] & (1 << 0)))
		{
			// Identity map the whole page table
			for (uint32_t i = 0; i < 1024; i++)
				pt[i] = address | (i << 12) | 0x03;
			// Set the pde to point to page table
			s_page_directory[pd_index] = (uint32_t)pt | 0x03;
			// Flush TLB
			for (uint32_t i = 0; i < 1024; i++)
				asm volatile("invlpg (%0)" :: "r" (address | (i << 12)) : "memory");
		}
	}

	void MapFramebuffer(uint32_t address)
	{
		MapPDE(address, s_page_table_framebuffer);
	}

	void MapRSDP(uint32_t address)
	{
		MapPDE(address, s_page_table_rsdp);
	}

	void MapAPIC(uint32_t address)
	{
		MapPDE(address, s_page_table_apic);
	}

}