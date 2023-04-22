#pragma once

#include <BAN/Formatter.h>
#include <kernel/PIT.h>

#define dprintln(...)																																\
	do {																																			\
		Debug::DebugLock::lock();																													\
		BAN::Formatter::print(Debug::putchar, "[{5}.{3}] {}:{}:  ", PIT::ms_since_boot() / 1000, PIT::ms_since_boot() % 1000, __FILE__, __LINE__);	\
		BAN::Formatter::print(Debug::putchar, __VA_ARGS__);																							\
		BAN::Formatter::print(Debug::putchar, "\r\n");																								\
		Debug::DebugLock::unlock();																													\
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

#define BOCHS_BREAK() asm volatile("xchgw %bx, %bx")

namespace Debug
{
	void dump_stack_trace();
	void putchar(char);

	class DebugLock
	{
	public:
		static void lock();
		static void unlock();
	};
}