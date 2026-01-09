#pragma once

#if __is_kernel

#include <kernel/Debug.h>

#else

#include <BAN/Formatter.h>
#include <stdio.h>

#define __debug_putchar [](int c) { putc_unlocked(c, stddbg); }

#define dprintln(...)											\
	do {														\
		flockfile(stddbg);										\
		BAN::Formatter::print(__debug_putchar, __VA_ARGS__);	\
		BAN::Formatter::print(__debug_putchar,"\n");			\
		fflush(stddbg);											\
		funlockfile(stddbg);									\
	} while (false)

#define dwarnln(...)											\
	do {														\
		flockfile(stddbg);										\
		BAN::Formatter::print(__debug_putchar, "\e[33m");		\
		BAN::Formatter::print(__debug_putchar, __VA_ARGS__);	\
		BAN::Formatter::print(__debug_putchar, "\e[m\n");		\
		fflush(stddbg);											\
		funlockfile(stddbg);									\
	} while(false)

#define derrorln(...)											\
	do {														\
		flockfile(stddbg);										\
		BAN::Formatter::print(__debug_putchar, "\e[31m");		\
		BAN::Formatter::print(__debug_putchar, __VA_ARGS__);	\
		BAN::Formatter::print(__debug_putchar, "\e[m\n");		\
		fflush(stddbg);											\
		funlockfile(stddbg);									\
	} while(false)

#define dprintln_if(cond, ...)		\
	do {							\
		if constexpr(cond)			\
			dprintln(__VA_ARGS__);	\
	} while(false)

#define dwarnln_if(cond, ...)		\
	do {							\
		if constexpr(cond)			\
			dwarnln(__VA_ARGS__);	\
	} while(false)

#define derrorln_if(cond, ...)		\
	do {							\
		if constexpr(cond)			\
			derrorln(__VA_ARGS__);	\
	} while(false)

#endif
