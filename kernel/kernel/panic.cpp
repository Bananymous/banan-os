#include <kernel/panic.h>
#include <kernel/tty.h>

namespace Kernel
{

	__attribute__((__noreturn__))
	void panic(const char* message)
	{
		terminal_writestring("Kernel panic: ");
		terminal_writestring(message);
		asm volatile("hlt");
		__builtin_unreachable();
	}

}
