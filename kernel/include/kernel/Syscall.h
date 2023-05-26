#pragma once

#include <kernel/Attributes.h>
#include <stdint.h>
#include <sys/syscall.h>

namespace Kernel
{

	ALWAYS_INLINE long syscall(int syscall, uintptr_t arg1 = 0, uintptr_t arg2 = 0, uintptr_t arg3 = 0, uintptr_t arg4 = 0, uintptr_t arg5 = 0)
	{
		long ret;
		asm volatile("int $0x80" : "=a"(ret) : "a"(syscall), "b"((uintptr_t)arg1), "c"((uintptr_t)arg2), "d"((uintptr_t)arg3), "S"((uintptr_t)arg4), "D"((uintptr_t)arg5) : "memory");
		return ret;
	}

}
