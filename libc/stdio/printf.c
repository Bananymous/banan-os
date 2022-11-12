#include <stdio.h>

int printf(const char* restrict fmt, ...)
{
	int len = 0;
	while (fmt[len])
		putchar(fmt[len++]);
	return len;
}