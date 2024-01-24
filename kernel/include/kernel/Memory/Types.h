#pragma once

#include <kernel/Arch.h>

#if ARCH(x86_64)
	#define KERNEL_OFFSET 0xFFFFFFFF80000000
	#define V2P(vaddr) (((vaddr_t)(vaddr)) - KERNEL_OFFSET)
	#define P2V(paddr) (((paddr_t)(paddr)) + KERNEL_OFFSET)
#else
	#error
#endif

#define PAGE_SIZE ((uintptr_t)4096)
#define PAGE_SIZE_SHIFT 12
#define PAGE_ADDR_MASK (~(uintptr_t)0xFFF)

namespace Kernel
{

	using vaddr_t = uintptr_t;
	using paddr_t = uintptr_t;

}
