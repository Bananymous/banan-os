#include <kernel/Debug.h>
#include <kernel/Serial.h>
#include <kernel/TTY.h>

namespace Debug
{

	void putchar(char ch)
	{
		if (Serial::IsInitialized())
			return Serial::putchar(ch);
		if (TTY::IsInitialized())
			return TTY::PutCharCurrent(ch);
	}

}