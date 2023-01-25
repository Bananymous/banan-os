#include <kernel/Panic.h>

namespace Kernel
{

	void dump_stacktrace()
	{
		struct stackframe
		{
			stackframe* ebp;
			uintptr_t eip;
		};

		stackframe* frame = (stackframe*)__builtin_frame_address(0);
		BAN::Formatter::print(Debug::putchar, "\e[36mStack trace:\r\n");
		while (frame)
		{
			BAN::Formatter::print(Debug::putchar, "    {}\r\n", (void*)frame->eip);
			frame = frame->ebp;
		}
	}

}