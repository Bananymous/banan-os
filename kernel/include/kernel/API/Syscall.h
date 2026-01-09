#pragma once

#include <kernel/Attributes.h>
#include <kernel/IDT.h>
#include <stdint.h>
#include <sys/syscall.h>

namespace Kernel
{

	ALWAYS_INLINE long syscall(int syscall, uintptr_t arg1 = 0, uintptr_t arg2 = 0, uintptr_t arg3 = 0, uintptr_t arg4 = 0, uintptr_t arg5 = 0)
	{
		long ret;
#if ARCH(x86_64)
		register uintptr_t r10 asm("r10") = arg3;
		register uintptr_t r8  asm( "r8") = arg4;
		register uintptr_t r9  asm( "r9") = arg5;
		asm volatile(
			"syscall"
			: "=a"(ret)
			, "+D"(syscall)
			, "+S"(arg1)
			, "+d"(arg2)
			, "+r"(r10)
			, "+r"(r8)
			, "+r"(r9)
			:: "rcx", "r11", "memory");
#elif ARCH(i686)
		asm volatile(
			"int %[irq]"
			: "=a"(ret)
			: [irq]"i"(static_cast<int>(IRQ_SYSCALL)) // WTF GCC 15
			, "a"(syscall)
			, "b"(arg1)
			, "c"(arg2)
			, "d"(arg3)
			, "S"(arg4)
			, "D"(arg5)
			: "memory");
#endif
		return ret;
	}

}
