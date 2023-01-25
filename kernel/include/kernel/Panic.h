#pragma once

#include <kernel/Debug.h>
#include <kernel/kprint.h>

#define Panic(...) PanicImpl(__FILE__, __LINE__, __VA_ARGS__)

namespace Kernel
{

	template<typename... Args>
	__attribute__((__noreturn__))
	static void PanicImpl(const char* file, int line, const char* message, Args... args)
	{
		derrorln("Kernel panic at {}:{}", file, line);
		derrorln(message, args...);
		Debug::DumpStackTrace();
		asm volatile("cli");
		for (;;)
			asm volatile("hlt");
		__builtin_unreachable();
	}

}