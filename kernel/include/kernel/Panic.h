#pragma once

#include <kernel/kprint.h>
#include <kernel/Serial.h>
#include <kernel/VESA.h>

#define Panic(...) PanicImpl(__FILE__, __LINE__, __VA_ARGS__)

namespace Kernel
{

	template<typename... Args>
	__attribute__((__noreturn__))
	static void PanicImpl(const char* file, int line, const char* message, Args... args)
	{
		if (VESA::IsInitialized())
		{
			kprint("\e[31mKernel panic at {}:{}\n", file, line);
			kprint(message, args...);
			kprint("\e[m\n");
		}
		else
		{
			derrorln("Kernel panic at {}:{}", file, line);
			derrorln(message, args...);
		}
		asm volatile("cli; hlt");
		__builtin_unreachable();
	}

}