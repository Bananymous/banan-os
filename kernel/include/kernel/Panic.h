#pragma once

#include <kernel/kprint.h>
#include <kernel/Serial.h>
#include <kernel/TTY.h>

#define Panic(...) PanicImpl(__FILE__, __LINE__, __VA_ARGS__)

namespace Kernel
{

	void dump_stacktrace();

	template<typename... Args>
	__attribute__((__noreturn__))
	static void PanicImpl(const char* file, int line, const char* message, Args... args)
	{
		derrorln("Kernel panic at {}:{}", file, line);
		derrorln(message, args...);
		dump_stacktrace();
		if (TTY::IsInitialized())
		{
			kprint("\e[31mKernel panic at {}:{}\n", file, line);
			kprint(message, args...);
			kprint("\e[m\n");
		}
		asm volatile("cli; hlt");
		__builtin_unreachable();
	}

}