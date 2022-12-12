#pragma once

#include <kernel/Formatter.h>
#include <kernel/tty.h>

#define kprint		Formatter::print<TTY::putchar>
#define kprintln	Formatter::println<TTY::putchar>
