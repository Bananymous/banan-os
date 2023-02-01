#include <kernel/Debug.h>
#include <kernel/Serial.h>
#include <kernel/TTY.h>

namespace Debug
{

	void dump_stack_trace()
	{
		struct stackframe
		{
			stackframe* rbp;
			uintptr_t rip;
		};

		stackframe* frame = (stackframe*)__builtin_frame_address(0);
		if (!frame)
		{
			dprintln("Could not get frame address");
			return;
		}
		uintptr_t first_rip = frame->rip;

		BAN::Formatter::print(Debug::putchar, "\e[36mStack trace:\r\n");
		while (frame)
		{
			BAN::Formatter::print(Debug::putchar, "    {}\r\n", (void*)frame->rip);
			frame = frame->rbp;

			if (frame && frame->rip == first_rip)
			{
				derrorln("looping kernel panic :(");
				return;
			}
		}
	}

	void putchar(char ch)
	{
		if (Serial::is_initialized())
			return Serial::putchar(ch);
		if (TTY::is_initialized())
			return TTY::putchar_current(ch);
	}

}