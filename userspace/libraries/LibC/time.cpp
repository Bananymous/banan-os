#include <BAN/Debug.h>
#include <BAN/Math.h>

#include <ctype.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

// sample implementation from https://pubs.opengroup.org/onlinepubs/9699919799/functions/asctime.html
char* asctime(const struct tm* timeptr)
{
	static constexpr char wday_name[][4] {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	};
	static constexpr char mon_name[][4] {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	static char result[128];
	sprintf(result, "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n",
		wday_name[timeptr->tm_wday],
		mon_name[timeptr->tm_mon],
		timeptr->tm_mday, timeptr->tm_hour,
		timeptr->tm_min, timeptr->tm_sec,
		1900 + timeptr->tm_year);
	return result;
}

int clock_gettime(clockid_t clock_id, struct timespec* tp)
{
	return syscall(SYS_CLOCK_GETTIME, clock_id, tp);
}

char* ctime(const time_t* clock)
{
	return asctime(localtime(clock));
}

int nanosleep(const struct timespec* rqtp, struct timespec* rmtp)
{
	return syscall(SYS_NANOSLEEP, rqtp, rmtp);
}

time_t time(time_t* tloc)
{
	timespec tp;
	if (clock_gettime(CLOCK_REALTIME, &tp) == -1)
		return -1;
	if (tloc)
		*tloc = tp.tv_sec;
	return tp.tv_sec;
}

struct tm* gmtime(const time_t* timer)
{
	static struct tm tm;

	constexpr auto is_leap_year =
		[](time_t year) -> bool
		{
			if (year % 400 == 0)
				return true;
			if (year % 100 == 0)
				return false;
			if (year % 4 == 0)
				return true;
			return false;
		};

	constexpr uint64_t month_days[] { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };

	time_t time = *timer;

	tm.tm_sec  = time % 60; time /= 60;
	tm.tm_min  = time % 60; time /= 60;
	tm.tm_hour = time % 24; time /= 24;

	time_t total_days = time;
	tm.tm_wday = (total_days + 4) % 7;
	tm.tm_year = 1970;
	while (total_days >= 365U + is_leap_year(tm.tm_year))
	{
		total_days -= 365U + is_leap_year(tm.tm_year);
		tm.tm_year++;
	}

	bool is_leap_day = is_leap_year(tm.tm_year) && total_days == month_days[2];
	bool had_leap_day = is_leap_year(tm.tm_year) && total_days > month_days[2];

	for (tm.tm_mon = 0; tm.tm_mon < 12; tm.tm_mon++)
		if (total_days < month_days[tm.tm_mon + 1] + (is_leap_day || had_leap_day))
			break;

	tm.tm_mday = total_days - month_days[tm.tm_mon] + !had_leap_day;
	tm.tm_yday = total_days;
	tm.tm_year -= 1900;
	tm.tm_isdst = 0;

	return &tm;
}

struct tm* localtime(const time_t* timer)
{
	// FIXME: support timezones
	return gmtime(timer);
}

size_t strftime(char* __restrict s, size_t maxsize, const char* __restrict format, const struct tm* __restrict timeptr)
{
	size_t len = 0;

	struct conversion_t
	{
		char flag = '\0';
		int width = -1;
		char modifier = '\0';
	};

	static constexpr const char* abbr_wday[] {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	};
	static constexpr const char* full_wday[] {
		"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
	};

	static constexpr const char* abbr_mon[] {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	static constexpr const char* full_mon[] {
		"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"
	};

	const auto append_string =
		[&](const char* string) -> bool
		{
			size_t string_len = strlen(string);
			if (len + string_len >= maxsize)
				return false;
			strcpy(s + len, string);
			len += string_len;
			return true;
		};

	const auto append_string_from_list =
		[&]<size_t LIST_SIZE>(int index, const char* const (&list)[LIST_SIZE]) -> bool
		{
			const char* string = "INVALID";
			if (index >= 0 && index < (int)LIST_SIZE)
				string = list[index];
			return append_string(string);
		};

	const auto append_value =
		[&](const char* format, int value) -> bool
		{
			int written = snprintf(s + len, maxsize - len, format, value);
			if (len + written >= maxsize)
				return false;
			len += written;
			return true;
		};

	while (*format && len < maxsize)
	{
		if (*format != '%')
		{
			s[len++] = *format++;
			continue;
		}

		format++;

		conversion_t conversion;
		switch (*format)
		{
			case '+':
			case '0':
				conversion.flag = *format;
				format++;
				break;
		}
		if (isdigit(*format))
		{
			conversion.width = 0;
			while (isdigit(*format))
			{
				conversion.width = (conversion.width * 10) + (*format + '0');
				format++;
			}
		}
		switch (*format)
		{
			case 'E':
			case 'O':
				dwarnln("TODO: strftime moodifiers");
				conversion.modifier = *format;
				format++;
				break;
		}

		switch (*format)
		{
			case 'a':
				if (!append_string_from_list(timeptr->tm_wday, abbr_wday))
					return 0;
				break;
			case 'A':
				if (!append_string_from_list(timeptr->tm_wday, full_wday))
					return 0;
				break;
			case 'b':
			case 'h':
				if (!append_string_from_list(timeptr->tm_mon, abbr_mon))
					return 0;
				break;
			case 'B':
				if (!append_string_from_list(timeptr->tm_mon, full_mon))
					return 0;
				break;
			case 'c':
				if (size_t ret = strftime(s + len, maxsize - len, "%a %b %e %H:%M:%S %Y", timeptr))
					len += ret;
				else return 0;
				break;
			case 'C':
			{
				if (conversion.flag == '\0')
					conversion.flag = ' ';
				if (conversion.flag == '+' && conversion.width <= 2)
					conversion.flag = '0';
				if (conversion.width < 2)
					conversion.width = 2;

				char new_format[32];
				sprintf(new_format, "%%%c%dd", conversion.flag, conversion.width);
				if (!append_value(new_format, timeptr->tm_year % 100))
					return 0;
				break;
			}
			case 'd':
				if (!append_value("%02d", timeptr->tm_mday))
					return 0;
				break;
			case 'D':
				if (size_t ret = strftime(s + len, maxsize - len, "%m/%d/%y", timeptr))
					len += ret;
				else return 0;
				break;
			case 'e':
				if (!append_value("% 2d", timeptr->tm_mday))
					return 0;
				break;
			case 'F':
			{
				if (conversion.flag == '\0')
					conversion.flag = '+';
				if (conversion.width == -1)
					conversion.width = 10;
				if (conversion.width < 6)
					conversion.width = 6;

				char new_format[32];
				sprintf(new_format, "%%%c%dY-%%m-%%d", conversion.flag, conversion.width - 6);

				if (size_t ret = strftime(s + len, maxsize - len, new_format, timeptr))
					len += ret;
				else return 0;
				break;
			}
			case 'H':
				if (!append_value("%02d", timeptr->tm_hour))
					return 0;
				break;
			case 'I':
				if (!append_value("%02d", ((timeptr->tm_hour + 11) % 12) + 1))
					return 0;
				break;
			case 'j':
				if (!append_value("%03d", timeptr->tm_yday + 1))
					return 0;
				break;
			case 'm':
				if (!append_value("%02d", timeptr->tm_mon + 1))
					return 0;
				break;
			case 'M':
				if (!append_value("%02d", timeptr->tm_min))
					return 0;
				break;
			case 'n':
				s[len++] = '\n';
				break;
			case 'p':
				if (!append_string(timeptr->tm_hour < 12 ? "AM" : "PM"))
					return 0;
				break;
			case 'r':
				if (size_t ret = strftime(s + len, maxsize - len, "%I:%M:%S %p", timeptr))
					len += ret;
				else return 0;
				break;
			case 'R':
				if (size_t ret = strftime(s + len, maxsize - len, "%H:%M", timeptr))
					len += ret;
				else return 0;
				break;
			case 'S':
				if (!append_value("%02d", timeptr->tm_sec))
					return 0;
				break;
			case 't':
				s[len++] = '\t';
				break;
			case 'T':
				if (size_t ret = strftime(s + len, maxsize - len, "%H:%M:%S", timeptr))
					len += ret;
				else return 0;
				break;
			case 'u':
				if (!append_value("%d", ((timeptr->tm_wday + 6) % 7) + 1))
					return 0;
				break;
			case 'U':
				if (!append_value("%02d", (timeptr->tm_yday - timeptr->tm_wday + 7) / 7))
					return 0;
				break;
			case 'g':
			case 'G':
			case 'V':
			{
				// Adapted from GNU libc implementation

				constexpr auto is_leap_year =
					[](int year) -> bool
					{
						if (year % 400 == 0)
							return true;
						if (year % 100 == 0)
							return false;
						if (year % 4 == 0)
							return true;
						return false;
					};

				constexpr auto iso_week_days =
					[](int yday, int wday) -> int
					{
						return yday - (wday + 382) % 7 + 3;
					};

				int year = timeptr->tm_year + 1900;
				int days = iso_week_days(timeptr->tm_yday, timeptr->tm_wday);

				if (days < 0)
				{
					year--;
					days = iso_week_days(timeptr->tm_yday + (365 + is_leap_year(year)), timeptr->tm_wday);
				}
				else
				{
					int d = iso_week_days(timeptr->tm_yday - (365 + is_leap_year(year)), timeptr->tm_wday);
					if (d >= 0)
					{
						year++;
						days = d;
					}
				}

				switch (*format)
				{
					case 'g':
						if (!append_value("%02d", ((year % 100) + 100) % 100))
							return 0;
						break;
					case 'G':
					{
						if (conversion.flag == '\0')
							conversion.flag = ' ';
						if (conversion.flag == '+' && conversion.width <= 4)
							conversion.flag = '0';
						if (conversion.width == -1)
							conversion.width = 0;

						char new_format[32];
						sprintf(new_format, "%%%c%dd", conversion.flag, conversion.width);
						if (!append_value(new_format, year))
							return 0;
						break;
					}
					case 'V':
						if (!append_value("%02d", days / 7 + 1))
							return 0;
						break;
				}
				break;
			}
			case 'w':
				if (!append_value("%d", timeptr->tm_wday))
					return 0;
				break;
			case 'W':
				if (!append_value("%02d", (timeptr->tm_yday - (timeptr->tm_wday - 1 + 7) % 7 + 7) / 7))
					return 0;
				break;
			case 'x':
				if (size_t ret = strftime(s + len, maxsize - len, "%m/%d/%y", timeptr))
					len += ret;
				else return 0;
				break;
			case 'X':
				if (size_t ret = strftime(s + len, maxsize - len, "%H:%M:%S", timeptr))
					len += ret;
				else return 0;
				break;
			case 'y':
				if (!append_value("%d", timeptr->tm_yday % 100))
					return 0;
				break;
			case 'Y':
			{
				if (conversion.flag == '\0')
					conversion.flag = ' ';
				if (conversion.flag == '+' && conversion.width <= 4)
					conversion.flag = '0';
				if (conversion.width == -1)
					conversion.width = 0;

				char new_format[32];
				sprintf(new_format, "%%%c%dd", conversion.flag, conversion.width);
				if (!append_value(new_format, 1900 + timeptr->tm_year))
					return 0;
				break;
			}
			case 'z':
				// FIXME: support timezones
				break;
			case 'Z':
				// FIXME: support timezones
				break;
			case '%':
				s[len++] = '%';
				break;
		}

		format++;
	}

	if (*format != '\0')
		return 0;
	s[len++] = '\0';
	return len;
}
