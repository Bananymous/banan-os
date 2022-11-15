#include <stdio.h>

#if defined(__is_libk)
#include <kernel/tty.h>
#include <kernel/panic.h>
#else
#include <stdlib.h>
#endif

int putchar(int c)
{
#if defined(__is_libk)
	Kernel::panic("Please use kprint() instead of stdio");
	char ch = (char)c;
	terminal_write(&ch, sizeof(ch));
#else
	abort();
#endif
	return c;
}