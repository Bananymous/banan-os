#include <kernel/panic.h>
#include <kernel/kprint.h>

namespace Kernel
{

	__attribute__((__noreturn__))
	void panic(const char* message)
	{
		kprint("Kernel panic: {}", message);
		asm volatile("hlt");
		__builtin_unreachable();
	}

}
