#include <kernel/Debug.h>
#include <kernel/Serial.h>
#include <kernel/TTY.h>

namespace Debug
{

	void DumpStackTrace()
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

	void putchar(char ch)
	{
		if (Serial::IsInitialized())
			return Serial::putchar(ch);
		if (TTY::IsInitialized())
			return TTY::PutCharCurrent(ch);
	}

}