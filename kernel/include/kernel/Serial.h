#pragma once

#include <kernel/Printer.h>

#define dprint		Printer::print<Serial::serial_putc>
#define dprintln	Printer::println<Serial::serial_putc>

namespace Serial
{

	void initialize();

	void serial_putc(char);

}