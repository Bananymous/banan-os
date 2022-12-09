#include <stdio.h>

#if defined(__is_libk)
	#include <kernel/panic.h>
#else
	#include <stdlib.h>
#endif

int putchar(int c)
{
#if defined(__is_libk)
	Kernel::panic("Please use kprint() instead of stdio");
#else
	abort();
#endif
	return c;
}