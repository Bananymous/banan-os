#pragma once

#include <BAN/Formatter.h>

#include <stdint.h>

namespace BAN
{

	struct Time
	{
		uint32_t year;
		uint8_t	month;
		uint8_t	day;
		uint8_t	hour;
		uint8_t	minute;
		uint8_t	second;
		uint8_t week_day;
	};

	uint64_t to_unix_time(const BAN::Time&);
	BAN::Time from_unix_time(uint64_t);

}

namespace BAN::Formatter
{

	template<typename F>
	void print_argument(F putc, const Time& time, const ValueFormat&)
	{
		constexpr const char* week_days[]	{ "", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
		constexpr const char* months[]		{ "", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
		print(putc, "{} {} {} {2}:{2}:{2} GMT+0 {4}", week_days[time.week_day], months[time.month], time.day, time.hour, time.minute, time.second, time.year);
	}

}
