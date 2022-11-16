#pragma once

#include <kernel/kprint.h>

namespace Kernel
{

	template<typename... Args>
	__attribute__((__noreturn__))
	static void panic(const char* message, Args... args)
	{
		kprint("Kernel panic: ");
		kprint(message, args...);
		kprint("\n");
		asm volatile("hlt");
		__builtin_unreachable();
	}

}