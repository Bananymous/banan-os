#pragma once

#include <BAN/Formatter.h>
#include <kernel/PIT.h>

#define dprintln(...)																																		\
	do {																																					\
		BAN::Formatter::print(Serial::serial_putc, "[{5}.{3}] {}({}):  ", PIT::ms_since_boot() / 1000, PIT::ms_since_boot() % 1000, __FILE__, __LINE__);	\
		BAN::Formatter::println(Serial::serial_putc, __VA_ARGS__);																							\
	} while(false)

#define dwarnln(...)											\
	do {														\
		BAN::Formatter::print(Serial::serial_putc, "\e[33m");	\
		dprintln(__VA_ARGS__);									\
		BAN::Formatter::print(Serial::serial_putc, "\e[m");		\
	} while(false)

#define derrorln(...)											\
	do {														\
		BAN::Formatter::print(Serial::serial_putc, "\e[31m");	\
		dprintln(__VA_ARGS__);									\
		BAN::Formatter::print(Serial::serial_putc, "\e[m");		\
	} while(false)

namespace Serial
{

	void initialize();

	void serial_putc(char);

}