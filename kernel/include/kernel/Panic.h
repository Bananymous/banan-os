#pragma once

#include <kernel/kprint.h>
#include <kernel/Serial.h>
#include <kernel/TTY.h>

#define Panic(...) PanicImpl(__FILE__, __LINE__, __VA_ARGS__)

namespace Kernel
{

	static void dump_stacktrace()
	{
		struct stackframe
		{
			stackframe* ebp;
			uint32_t eip;
		};

		stackframe* frame;
		asm volatile("movl %%ebp, %0" : "=r"(frame));
		BAN::Formatter::println(Serial::serial_putc, "\e[36mStack trace:");
		while (frame)
		{
			BAN::Formatter::println(Serial::serial_putc, "    0x{8H}", frame->eip);
			frame = frame->ebp;
		}
	}

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