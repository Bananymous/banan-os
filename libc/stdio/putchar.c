#include <stdio.h>

#if defined(__is_libk)
#include <kernel/tty.h>
#endif

int putchar(int c)
{
#if defined(__is_libk)
	char ch = (char)c;
	terminal_write(&ch, sizeof(ch));
#else
#endif
	return c;
}