#pragma once

#include <BAN/Formatter.h>
#include <kernel/Debug.h>

// AML_DEBUG_LEVEL:
//   0: No debug output
//   1: Dump AML after parsing
//   2: Dump AML while parsing
#define AML_DEBUG_LEVEL 0

#define AML_TODO(...) 											\
	do {														\
		BAN::Formatter::print(Debug::putchar, "\e[33mTODO: ");	\
		BAN::Formatter::print(Debug::putchar, __VA_ARGS__);		\
		BAN::Formatter::println(Debug::putchar, "\e[m");		\
	} while (0)

#define AML_ERROR(...) 											\
	do {														\
		BAN::Formatter::print(Debug::putchar, "\e[31mERROR: ");	\
		BAN::Formatter::print(Debug::putchar, __VA_ARGS__);		\
		BAN::Formatter::println(Debug::putchar, "\e[m");		\
	} while (0)

#define AML_PRINT(...) BAN::Formatter::println(Debug::putchar, __VA_ARGS__)

#define AML_DEBUG_PRINT_INDENT(indent)			\
	do {										\
		for (int i = 0; i < (indent) * 2; i++)	\
			AML_DEBUG_PUTC(' ');				\
	} while (0)

#define AML_DEBUG_PUTC(c) Debug::putchar(c)
#define AML_DEBUG_PRINT(...) BAN::Formatter::print(Debug::putchar, __VA_ARGS__)
#define AML_DEBUG_PRINTLN(...) BAN::Formatter::println(Debug::putchar, __VA_ARGS__)
