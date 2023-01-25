#pragma once

#include <kernel/Debug.h>
#include <kernel/kprint.h>

#define Panic(...) PanicImpl(__FILE__, __LINE__, __VA_ARGS__)

namespace Kernel
{

	void dump_stacktrace();

	template<typename... Args>
	__attribute__((__noreturn__))
	static void PanicImpl(const char* file, int line, const char* message, Args... args)
	{
		kprintln("\e[31mKernel panic at {}:{}\e[m", file, line);
		derrorln(message, args...);
		dump_stacktrace();
		asm volatile("cli");
		for (;;)
			asm volatile("hlt");
		__builtin_unreachable();
	}

}