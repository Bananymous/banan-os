#pragma once

#include <stdint.h>

namespace RTC
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

	Time GetCurrentTime();

}