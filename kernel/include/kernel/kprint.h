#pragma once

#include <BAN/Formatter.h>
#include <kernel/TTY.h>

#define kprint(...)			BAN::Formatter::print(TTY::PutCharCurrent, __VA_ARGS__)
#define kprintln(...)	BAN::Formatter::println(TTY::PutCharCurrent, __VA_ARGS__)
