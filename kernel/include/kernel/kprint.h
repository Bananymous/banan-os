#pragma once

#include <BAN/Formatter.h>
#include <kernel/Terminal/TTY.h>

#define kprint(...)		BAN::Formatter::print(Kernel::TTY::putchar_current, __VA_ARGS__)
#define kprintln(...)	BAN::Formatter::println(Kernel::TTY::putchar_current, __VA_ARGS__)
