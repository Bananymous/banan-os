#pragma once

#include <BAN/Formatter.h>
#include <kernel/tty.h>

#define kprint		BAN::Formatter::print<TTY::putchar>
#define kprintln	BAN::Formatter::println<TTY::putchar>
