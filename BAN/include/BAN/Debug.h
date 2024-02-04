#pragma once

#if __is_kernel
#error "This is userspace only file"
#endif

#include <BAN/Formatter.h>
#include <stdio.h>

#define __debug_putchar [](int c) { putc(c, stddbg); }

#define dprintln(...)											\
	do {														\
		BAN::Formatter::print(__debug_putchar, __VA_ARGS__);	\
		BAN::Formatter::print(__debug_putchar,"\r\n");			\
		fflush(stddbg);											\
	} while (false)

#define dwarnln(...)										\
	do {													\
		BAN::Formatter::print(__debug_putchar, "\e[33m");	\
		dprintln(__VA_ARGS__);								\
		BAN::Formatter::print(__debug_putchar, "\e[m");		\
	} while(false)

#define derrorln(...)										\
	do {													\
		BAN::Formatter::print(__debug_putchar, "\e[31m");	\
		dprintln(__VA_ARGS__);								\
		BAN::Formatter::print(__debug_putchar, "\e[m");		\
	} while(false)
