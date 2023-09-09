#include <BAN/Time.h>

namespace BAN
{

	static constexpr bool is_leap_year(uint64_t year)
	{
		if (year % 400 == 0)
			return true;
		if (year % 100 == 0)
			return false;
		if (year % 4 == 0)
			return true;
		return false;
	}

	static constexpr uint64_t leap_days_since_epoch(const BAN::Time& time)
	{
		uint64_t leap_years = 0;
		for (uint32_t year = 1970; year < time.year; year++)
			if (is_leap_year(year))
				leap_years++;
		if (is_leap_year(time.year) && time.month >= 3)
			leap_years++;
		return leap_years;
	}

	static constexpr uint64_t month_days[] { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

	uint64_t to_unix_time(const BAN::Time& time)
	{
		uint64_t years = time.year - 1970;
		uint64_t days = years * 365 + month_days[time.month - 1] + leap_days_since_epoch(time) + (time.day - 1);
		uint64_t hours = days * 24 + time.hour;
		uint64_t minutes = hours * 60 + time.minute;
		uint64_t seconds = minutes * 60 + time.second;
		return seconds;
	}

	BAN::Time from_unix_time(uint64_t unix_time)
	{		
		BAN::Time time {};

		time.second = unix_time % 60; unix_time /= 60;
		time.minute = unix_time % 60; unix_time /= 60;
		time.hour   = unix_time % 24; unix_time /= 24;

		uint64_t total_days = unix_time;

		time.week_day = (total_days + 4) % 7 + 1;

		time.year = 1970;
		while (total_days >= 365U + is_leap_year(time.year))
		{
			total_days -= 365U + is_leap_year(time.year);
			time.year++;
		}

		bool is_leap_day = is_leap_year(time.year) && total_days == month_days[2];
		bool had_leap_day = is_leap_year(time.year) && total_days > month_days[2];

		for (time.month = 1; time.month < 12; time.month++)
			if (total_days < month_days[time.month] + (is_leap_day || had_leap_day))
				break;

		time.day = total_days - month_days[time.month - 1] + !had_leap_day;

		return time;
	}

}