#pragma once

#include <kernel/Debug.h>

#define panic(...) detail::panic_impl(__FILE__, __LINE__, __VA_ARGS__)

namespace Kernel
{

	namespace detail
	{
		template<typename... Args>
		__attribute__((__noreturn__))
		static void panic_impl(const char* file, int line, const char* message, Args... args)
		{
			asm volatile("cli");
			derrorln("Kernel panic at {}:{}", file, line);
			derrorln(message, args...);
			Debug::dump_stack_trace();
			for (;;)
				asm volatile("hlt");
			__builtin_unreachable();
		}
	}

}