#pragma once

#define SYS_EXIT 1
#define SYS_READ 2
#define SYS_WRITE 3

#include <stdint.h>

namespace Kernel
{

	template<typename T1 = void*, typename T2 = void*, typename T3 = void*>
	inline long syscall(int syscall, T1 arg1 = nullptr, T2 arg2 = nullptr, T3 arg3 = nullptr)
	{
		long ret;
		asm volatile("int $0x80" : "=a"(ret) : "a"(syscall), "b"((uintptr_t)arg1), "c"((uintptr_t)arg2), "d"((uintptr_t)arg3) : "memory");
		return ret;
	}

}
