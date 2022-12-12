#pragma once

#include <kernel/Formatter.h>

#define dprint		Formatter::print<Serial::serial_putc>
#define dprintln	Formatter::println<Serial::serial_putc>

namespace Serial
{

	void initialize();

	void serial_putc(char);

}