#pragma once

#if defined(__is_kernel)
	#include <kernel/Panic.h>
	#define ASSERT(cond) do { if (!(cond)) Kernel::panic("ASSERT("#cond") failed"); } while(false)
#else
	#error "NOT IMPLEMENTED"
#endif