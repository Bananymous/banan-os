#pragma once

#include <BAN/Formatter.h>

#include <stdint.h>

namespace BAN
{

	struct Time
	{
		uint8_t	second;
		uint8_t	minute;
		uint8_t	hour;
		uint8_t	day;
		uint8_t	month;
		int		year;
	};

}

namespace BAN::Formatter
{

	template<void(*PUTC_LIKE)(char)> void print_argument_impl(const Time& time, const ValueFormat&)
	{
		print<PUTC_LIKE>("{2}:{2}:{2} {2}.{2}.{4}", time.hour, time.minute, time.second, time.day, time.month, time.year);
	}

}