#include <kernel/Panic.h>

namespace Kernel
{

	void dump_stacktrace()
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
			BAN::Formatter::println(Serial::serial_putc, "    {}", (void*)frame->eip);
			frame = frame->ebp;
		}
	}

}