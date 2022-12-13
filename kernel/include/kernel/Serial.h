#pragma once

#include <BAN/Formatter.h>

#define dprint		BAN::Formatter::print<Serial::serial_putc>
#define dprintln	BAN::Formatter::println<Serial::serial_putc>

namespace Serial
{

	void initialize();

	void serial_putc(char);

}