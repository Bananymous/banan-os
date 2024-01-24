#pragma once

#include <BAN/Formatter.h>

#define dprintln(...)										\
	do {													\
		Debug::DebugLock::lock();							\
		Debug::print_prefix(__FILE__, __LINE__);				\
		BAN::Formatter::print(Debug::putchar, __VA_ARGS__);	\
		BAN::Formatter::print(Debug::putchar, "\r\n");		\
		Debug::DebugLock::unlock();							\
	} while(false)

#define dwarnln(...)										\
	do {													\
		Debug::DebugLock::lock();							\
		BAN::Formatter::print(Debug::putchar, "\e[33m");	\
		dprintln(__VA_ARGS__);								\
		BAN::Formatter::print(Debug::putchar, "\e[m");		\
		Debug::DebugLock::unlock();							\
	} while(false)

#define derrorln(...)										\
	do {													\
		Debug::DebugLock::lock();							\
		BAN::Formatter::print(Debug::putchar, "\e[31m");	\
		dprintln(__VA_ARGS__);								\
		BAN::Formatter::print(Debug::putchar, "\e[m");		\
		Debug::DebugLock::unlock();							\
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

#define BOCHS_BREAK() asm volatile("xchgw %bx, %bx")

namespace Debug
{
	void dump_stack_trace();
	void putchar(char);
	void print_prefix(const char*, int);

	class DebugLock
	{
	public:
		static void lock();
		static void unlock();
	};
}
