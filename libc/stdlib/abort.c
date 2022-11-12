#include <stdlib.h>
#include <stdio.h>

__attribute__((__noreturn__))
void abort(void)
{
#if defined(__is_libk)
	printf("Kernel panic: abort()\n");
	asm volatile("hlt");
#else
	printf("abort()\n");
#endif
	while (1);
	__builtin_unreachable();
}