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
		uint8_t week_day;
		uint8_t	day;
		uint8_t	month;
		int		year;
	};

}

namespace BAN::Formatter
{

	template<void(*PUTC_LIKE)(char)> void print_argument_impl(const Time& time, const ValueFormat&)
	{
		constexpr const char* week_days[]	{ "", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
		constexpr const char* months[]		{ "", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
		print<PUTC_LIKE>("{} {} {} {2}:{2}:{2} GMT+0 {4}", week_days[time.week_day], months[time.month], time.day, time.hour, time.minute, time.second, time.year);
	}

}