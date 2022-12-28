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

	void MapFramebuffer(uint32_t address)
	{
		uint32_t pd_index = address >> 22;

		if (!(s_page_directory[pd_index] & (1 << 0)))
		{
			for (uint32_t i = 0; i < 1024; i++)
				s_page_table_framebuffer[i] = address | (i << 12) | 0x03;
			s_page_directory[pd_index] = (uint32_t)s_page_table_framebuffer | 0x03;
			for (uint32_t i = 0; i < 1024; i++)
				asm volatile("invlpg (%0)" :: "r" (address | (i << 12)) : "memory");
		}
	}

	void MapRSDP(uint32_t address)
	{
		uint32_t pd_index = address >> 22;

		if (!(s_page_directory[pd_index] & (1 << 0)))
		{
			for (uint32_t i = 0; i < 1024; i++)
				s_page_table_rsdp[i] = address | (i << 12) | 0x03;
			s_page_directory[pd_index] = (uint32_t)s_page_table_rsdp | 0x03;
			for (uint32_t i = 0; i < 1024; i++)
				asm volatile("invlpg (%0)" :: "r" (address | (i << 12)) : "memory");
		}
	}

	void MapAPIC(uint32_t address)
	{
		uint32_t pd_index = address >> 22;

		if (!(s_page_directory[pd_index] & (1 << 0)))
		{
			for (uint32_t i = 0; i < 1024; i++)
				s_page_table_apic[i] = address | (i << 12) | 0x03;
			s_page_directory[pd_index] = (uint32_t)s_page_table_apic | 0x03;
			for (uint32_t i = 0; i < 1024; i++)
				asm volatile("invlpg (%0)" :: "r" (address | (i << 12)) : "memory");
		}
	}

}