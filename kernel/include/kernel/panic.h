#pragma once

#include <kernel/kprint.h>

namespace Kernel
{

	template<typename... Args>
	__attribute__((__noreturn__))
	static void panic(const char* message, Args... args)
	{
		kprint("\e[31mKernel panic: ");
		kprint(message, args...);
		kprint("\e[m\n");
		asm volatile("cli; hlt");
		__builtin_unreachable();
	}

}