#pragma once

#if __is_kernel

#include <kernel/Debug.h>

#else

#include <BAN/Formatter.h>
#include <stdio.h>

#define __debug_putchar [](int c) { putc(c, stddbg); }

#define dprintln(...)											\
	do {														\
		BAN::Formatter::print(__debug_putchar, __VA_ARGS__);	\
		BAN::Formatter::print(__debug_putchar,"\r\n");			\
		fflush(stddbg);											\
	} while (false)

#define dwarnln(...)											\
	do {														\
		BAN::Formatter::print(__debug_putchar, "\e[33m");		\
		BAN::Formatter::print(__debug_putchar, __VA_ARGS__);	\
		BAN::Formatter::print(__debug_putchar, "\e[m\r\n");		\
		fflush(stddbg);											\
	} while(false)

#define derrorln(...)											\
	do {														\
		BAN::Formatter::print(__debug_putchar, "\e[31m");		\
		BAN::Formatter::print(__debug_putchar, __VA_ARGS__);	\
		BAN::Formatter::print(__debug_putchar, "\e[m\r\n");		\
		fflush(stddbg);											\
	} while(false)

#endif
