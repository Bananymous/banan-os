#pragma once

#include <kernel/Debug.h>

#define __panic_stringify_helper(s) #s
#define __panic_stringify(s) __panic_stringify_helper(s)

#define panic(...) panic_impl(__FILE__ ":" __panic_stringify(__LINE__), __VA_ARGS__)

namespace Kernel
{

	extern volatile bool g_paniced;

	template<typename... Args>
	__attribute__((__noreturn__))
	static void panic_impl(const char* location, const char* message, Args&&... args)
	{
		asm volatile("cli");

		const bool had_debug_lock = Debug::s_debug_lock.current_processor_has_lock();
		derrorln("Kernel panic at {}", location);
		if (had_debug_lock)
			derrorln("  while having debug lock...");
		derrorln(message, BAN::forward<Args>(args)...);
		if (!g_paniced)
		{
			g_paniced = true;
			Debug::dump_stack_trace();
		}
		asm volatile("ud2");
		__builtin_unreachable();
	}

}
