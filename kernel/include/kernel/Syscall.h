#pragma once

#define SYS_EXIT 1
#define SYS_READ 2
#define SYS_WRITE 3
#define SYS_TERMID 4
#define SYS_CLOSE 5
#define SYS_OPEN 6
#define SYS_ALLOC 7
#define SYS_FREE 8
#define SYS_SEEK 9
#define SYS_TELL 10
#define SYS_GET_TERMIOS 11
#define SYS_SET_TERMIOS 12

#include <kernel/Attributes.h>
#include <stdint.h>

namespace Kernel
{

	ALWAYS_INLINE long syscall(int syscall, uintptr_t arg1 = 0, uintptr_t arg2 = 0, uintptr_t arg3 = 0, uintptr_t arg4 = 0, uintptr_t arg5 = 0)
	{
		long ret;
		asm volatile("int $0x80" : "=a"(ret) : "a"(syscall), "b"((uintptr_t)arg1), "c"((uintptr_t)arg2), "d"((uintptr_t)arg3), "S"((uintptr_t)arg4), "D"((uintptr_t)arg5) : "memory");
		return ret;
	}

}
