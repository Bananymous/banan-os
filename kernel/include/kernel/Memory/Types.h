#pragma once

#define PAGE_SIZE 4096
#define PAGE_FLAG_MASK ((uintptr_t)0xFFF)
#define PAGE_ADDR_MASK (~PAGE_FLAG_MASK)

namespace Kernel
{

	using vaddr_t = uintptr_t;
	using paddr_t = uintptr_t;
	
}