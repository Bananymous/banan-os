#include <kernel/Debug.h>
#include <kernel/Serial.h>
#include <kernel/Terminal/TTY.h>

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
		uintptr_t last_rip = 0;

		BAN::Formatter::print(Debug::putchar, "\e[36mStack trace:\r\n");
		while (frame)
		{
			BAN::Formatter::print(Debug::putchar, "    {}\r\n", (void*)frame->rip);
			frame = frame->rbp;

			if (frame && frame->rip == first_rip)
			{
				derrorln("looping kernel panic :(");
				break;
			}

			if (frame && frame->rip == last_rip)
			{
				derrorln("repeating stack strace");
				break;
			}

			last_rip = frame->rip;
		}
		BAN::Formatter::print(Debug::putchar, "\e[m");
	}

	void putchar(char ch)
	{
		if (Serial::is_initialized())
			return Serial::putchar(ch);
		if (Kernel::TTY::is_initialized())
			return Kernel::TTY::putchar_current(ch);
	}

}