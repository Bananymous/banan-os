#pragma once

#include <BAN/Formatter.h>
#include <kernel/TTY.h>

#define kprint(...)		BAN::Formatter::print(TTY::putchar_current, __VA_ARGS__)
#define kprintln(...)	BAN::Formatter::println(TTY::putchar_current, __VA_ARGS__)
