#pragma once

#include <BAN/Formatter.h>
#include <kernel/tty.h>

#define kprint(...) BAN::Formatter::print<TTY::putchar>(__VA_ARGS__)
#define kprintln(...) BAN::Formatter::println<TTY::putchar>(__VA_ARGS__)
