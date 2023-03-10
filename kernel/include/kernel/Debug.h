#pragma once

#include <BAN/Formatter.h>
#include <kernel/PIT.h>

#define dprintln(...)																																\
	do {																																			\
		BAN::Formatter::print(Debug::putchar, "[{5}.{3}] {}:{}:  ", PIT::ms_since_boot() / 1000, PIT::ms_since_boot() % 1000, __FILE__, __LINE__);	\
		BAN::Formatter::print(Debug::putchar, __VA_ARGS__);																							\
		BAN::Formatter::print(Debug::putchar, "\r\n");																								\
	} while(false)

#define dwarnln(...)											\
	do {														\
		BAN::Formatter::print(Debug::putchar, "\e[33m");	\
		dprintln(__VA_ARGS__);									\
		BAN::Formatter::print(Debug::putchar, "\e[m");		\
	} while(false)

#define derrorln(...)											\
	do {														\
		BAN::Formatter::print(Debug::putchar, "\e[31m");	\
		dprintln(__VA_ARGS__);									\
		BAN::Formatter::print(Debug::putchar, "\e[m");		\
	} while(false)

#define BOCHS_BREAK() asm volatile("xchgw %bx, %bx")

namespace Debug
{
	void dump_stack_trace();
	void putchar(char);
}