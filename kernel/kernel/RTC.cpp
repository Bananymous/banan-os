#include <kernel/IO.h>
#include <kernel/RTC.h>

#include <string.h>

#define CURRENT_YEAR 2022

#define CMOS_ADDRESS			0x70
#define CMOS_DATA				0x71

#define CMOS_REGISTER_SECOND	0x00
#define CMOS_REGISTER_MINUTE	0x02
#define CMOS_REGISTER_HOUR		0x04
#define CMOS_REGISTER_WEEK_DAY	0x06
#define CMOS_REGISTER_DAY		0x07
#define CMOS_REGISTER_MONTH		0x08
#define CMOS_REGISTER_YEAR		0x09
#define CMOS_REGISTER_STATUS_1	0x0A
#define CMOS_REGISTER_STATUS_2	0x0B

namespace RTC
{

	static bool get_update_in_progress()
	{
		IO::outb(CMOS_ADDRESS, CMOS_REGISTER_STATUS_1);
		return IO::inb(CMOS_DATA) & 0x80;
	}

	static uint8_t get_rtc_register(uint8_t reg)
	{
		IO::outb(CMOS_ADDRESS, reg);
		return IO::inb(CMOS_DATA);
	}

	static void get_time(BAN::Time& out)
	{
		out.second	= get_rtc_register(CMOS_REGISTER_SECOND);
		out.minute	= get_rtc_register(CMOS_REGISTER_MINUTE);
		out.hour	= get_rtc_register(CMOS_REGISTER_HOUR);
		out.day		= get_rtc_register(CMOS_REGISTER_DAY);
		out.month	= get_rtc_register(CMOS_REGISTER_MONTH);
		out.year	= get_rtc_register(CMOS_REGISTER_YEAR);
	}

	BAN::Time GetCurrentTime()
	{
		BAN::Time last_time	= {};
		BAN::Time time		= {};

		while (get_update_in_progress())
			continue;

		get_time(time);
		last_time.second = time.second + 1;

		while (memcmp(&last_time, &time, sizeof(BAN::Time)))
		{
			last_time = time;
			get_time(time);
		}
		
		uint8_t regB = get_rtc_register(0x0B);

		// Convert BCD to binary values if necessary
		if (!(regB & 0x04))
		{
            time.second	=  (time.second	& 0x0F) + ((time.second	/ 16) * 10);
            time.minute	=  (time.minute	& 0x0F) + ((time.minute	/ 16) * 10);
            time.hour	= ((time.hour	& 0x0F) + (((time.hour & 0x70) / 16) * 10) ) | (time.hour & 0x80);
            time.day	=  (time.day	& 0x0F) + ((time.day	/ 16) * 10);
            time.month	=  (time.month	& 0x0F) + ((time.month	/ 16) * 10);
            time.year	=  (time.year	& 0x0F) + ((time.year	/ 16) * 10);
		}

		// Convert 12 hour clock to 24 hour clock if necessary
		if (!(regB & 0x02) && (time.hour & 0x80))
			time.hour = ((time.hour & 0x7F) + 12) % 24;

		// Calculate the full 4 digit year
		time.year += (CURRENT_YEAR / 100) * 100;
		if (time.year < CURRENT_YEAR)
			time.year += 100;

		return time;
	}

}