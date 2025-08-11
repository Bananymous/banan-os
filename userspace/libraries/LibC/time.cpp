#include <BAN/Debug.h>
#include <BAN/Math.h>

#include <ctype.h>
#include <errno.h>
#include <langinfo.h>
#include <pthread.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

int daylight;
long timezone;
char* tzname[2];

int clock_gettime(clockid_t clock_id, struct timespec* tp)
{
	return syscall(SYS_CLOCK_GETTIME, clock_id, tp);
}

int clock_getres(clockid_t clock_id, struct timespec* res)
{
	(void)clock_id;
	res->tv_sec = 0;
	res->tv_nsec = 10;
	dprintln("TODO: clock_getres");
	return 0;
}

int nanosleep(const struct timespec* rqtp, struct timespec* rmtp)
{
	pthread_testcancel();
	return syscall(SYS_NANOSLEEP, rqtp, rmtp);
}

clock_t clock(void)
{
	timespec ts;
	if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) == -1)
		return -1;
	return ((uint64_t)ts.tv_sec  * CLOCKS_PER_SEC)
	     + ((uint64_t)ts.tv_nsec * CLOCKS_PER_SEC / 1'000'000'000);
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

double difftime(time_t time1, time_t time0)
{
	return time1 - time0;
}

// sample implementation from https://pubs.opengroup.org/onlinepubs/9699919799/functions/asctime.html
char* asctime_r(const struct tm* __restrict tm, char* __restrict buf)
{
	static constexpr char wday_name[][4] {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	};
	static constexpr char mon_name[][4] {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	sprintf(buf, "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n",
		wday_name[tm->tm_wday],
		mon_name[tm->tm_mon],
		tm->tm_mday, tm->tm_hour,
		tm->tm_min, tm->tm_sec,
		1900 + tm->tm_year);
	return buf;
}

char* asctime(const struct tm* timeptr)
{
	static char buf[26];
	return asctime_r(timeptr, buf);
}

char* ctime_r(const time_t* clock, char* buf)
{
	struct tm local;
	return asctime_r(localtime_r(clock, &local), buf);
}

char* ctime(const time_t* clock)
{
	static char buf[26];
	return ctime_r(clock, buf);
}

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

time_t mktime(struct tm* tm)
{
	if (tm->tm_year < 70)
	{
		errno = EOVERFLOW;
		return -1;
	}

	tm->tm_min += tm->tm_sec / 60;
	tm->tm_sec %= 60;

	tm->tm_hour += tm->tm_min / 60;
	tm->tm_min %= 60;

	tm->tm_mday += tm->tm_hour / 24;
	tm->tm_hour %= 24;

	static constexpr int month_days[] { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	for (;;)
	{
		int days_in_month = month_days[tm->tm_mon];
		if (tm->tm_mon == 1 && is_leap_year(tm->tm_year))
			days_in_month++;

		if (tm->tm_mday <= days_in_month)
			break;

		tm->tm_mday -= days_in_month;
		tm->tm_mon++;
	}

	tm->tm_year += tm->tm_mon / 12;
	tm->tm_mon %= 12;

	tm->tm_yday = tm->tm_mday - 1;
	for (int i = 0; i < tm->tm_mon; i++)
		tm->tm_yday += month_days[i];

	const time_t num_febs = (tm->tm_mon > 1) ? tm->tm_year + 1 : tm->tm_year;
	const time_t leap_years = (num_febs - 69) / 4 - (num_febs - 1) / 100 + (num_febs + 299) / 400;

	const time_t years = tm->tm_year - 70;
	const time_t days = years * 365 + leap_years + tm->tm_yday;
	const time_t hours = days * 24 + tm->tm_hour;
	const time_t minutes = hours * 60 + tm->tm_min;
	const time_t seconds = minutes * 60 + tm->tm_sec;

	tm->tm_wday = (days + 4) % 7;

	return seconds;
}

struct tm* gmtime_r(const time_t* timer, struct tm* __restrict result)
{
	constexpr uint64_t month_days[] { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };

	time_t time = *timer;

	result->tm_sec  = time % 60; time /= 60;
	result->tm_min  = time % 60; time /= 60;
	result->tm_hour = time % 24; time /= 24;

	time_t total_days = time;
	result->tm_wday = (total_days + 4) % 7;
	result->tm_year = 1970;
	while (total_days >= 365U + is_leap_year(result->tm_year))
	{
		total_days -= 365U + is_leap_year(result->tm_year);
		result->tm_year++;
	}

	bool is_leap_day = is_leap_year(result->tm_year) && total_days == month_days[2];
	bool had_leap_day = is_leap_year(result->tm_year) && total_days > month_days[2];

	for (result->tm_mon = 0; result->tm_mon < 12; result->tm_mon++)
		if (total_days < month_days[result->tm_mon + 1] + (is_leap_day || had_leap_day))
			break;

	result->tm_mday = total_days - month_days[result->tm_mon] + !had_leap_day;
	result->tm_yday = total_days;
	result->tm_year -= 1900;
	result->tm_isdst = 0;

	result->tm_gmtoff = 0;
	result->tm_zone = "UTC";

	return result;
}

struct tm* gmtime(const time_t* timer)
{
	static struct tm tm;
	return gmtime_r(timer, &tm);
}

struct tm* localtime_r(const time_t* timer, struct tm* result)
{
	// FIXME: support timezones
	return gmtime_r(timer, result);
}

struct tm* localtime(const time_t* timer)
{
	static struct tm tm;
	return localtime_r(timer, &tm);
}

void tzset()
{
	daylight = 0;
	timezone = 0;
	tzname[0] = const_cast<char*>("UTC");
	tzname[1] = const_cast<char*>("UTC");
}

size_t strftime(char* __restrict s, size_t maxsize, const char* __restrict format, const struct tm* __restrict timeptr)
{
	size_t len = 0;

	struct conversion_t
	{
		int flag = '\0';
		int width = -1;
		char modifier = '\0';
	};

	const auto append_string =
		[&s, &len, &maxsize](const char* string) -> bool
		{
			const size_t string_len = strlen(string);
			if (len + string_len >= maxsize)
				return false;
			strcpy(s + len, string);
			len += string_len;
			return true;
		};

	const auto append_month =
		[&append_string](int index, bool abbreviated)
		{
			const nl_item base = abbreviated ? ABMON_1 : MON_1;
			const char* string = "INVALID";
			if (index >= 0 && index < 12)
				string = nl_langinfo(base + index);
			return append_string(string);
		};

	const auto append_weekday =
		[&append_string](int index, bool abbreviated)
		{
			const nl_item base = abbreviated ? ABDAY_1 : DAY_1;
			const char* string = "INVALID";
			if (index >= 0 && index < 7)
				string = nl_langinfo(base + index);
			return append_string(string);
		};

	const auto append_value =
		[&s, &len, &maxsize]<typename T>(const char* format, T value) -> bool
		{
			const int written = snprintf(s + len, maxsize - len, format, value);
			if (len + written >= maxsize)
				return false;
			len += written;
			return true;
		};

	const auto append_value_weird =
		[&append_string, &append_value](long long value, int flag, int width) -> bool
		{
			char format[32];
			char* ptr = format;
			*ptr++ = '%';
			if (flag == '+')
				*ptr++ = '+';
			*ptr++ = '0';
			if (width != -1)
				ptr += sprintf(ptr, "%d", width);
			*ptr++ = 'l';
			*ptr++ = 'l';
			*ptr++ = 'd';
			*ptr++ = '\0';

			// idk why but musl libc test says that +4Y -> 2016 and +10F -> 2016-01-03
			// i have no idea why the + is not printed in those cases :)
			if (width < 11 && flag == '+')
			{
				char temp_buffer[12];
				int nprint = sprintf(temp_buffer, format, value);
				return append_string(temp_buffer + (nprint == width + 1));
			}

			return append_value(format, value);
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
		if (*format == '0' || *format == '+')
			conversion.flag = *format++;
		if (isdigit(*format))
		{
			conversion.width = 0;
			while (isdigit(*format))
			{
				conversion.width = (conversion.width * 10) + (*format - '0');
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
				if (!append_weekday(timeptr->tm_wday, true))
					return 0;
				break;
			case 'A':
				if (!append_weekday(timeptr->tm_wday, false))
					return 0;
				break;
			case 'b':
			case 'h':
				if (!append_month(timeptr->tm_mon, true))
					return 0;
				break;
			case 'B':
				if (!append_month(timeptr->tm_mon, false))
					return 0;
				break;
			case 'c':
				if (size_t ret = strftime(s + len, maxsize - len, nl_langinfo(D_T_FMT), timeptr))
					len += ret;
				else return 0;
				break;
			case 'C':
			{
				if (conversion.width == -1)
					conversion.width = 2;

				char new_format[32];
				if (conversion.flag == '+')
					sprintf(new_format, "%%+0%dd", conversion.width);
				else
					sprintf(new_format, "%%0%dd", conversion.width);
				if (!append_value(new_format, (1900 + timeptr->tm_year) / 100))
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
				if (!append_value("%2d", timeptr->tm_mday))
					return 0;
				break;
			case 'F':
			{
				// remove trailing "-mm-dd" from width
				if (conversion.width >= 6)
					conversion.width -= 6;

				char new_format[32];
				char* ptr = new_format;

				*ptr++ = '%';
				if (conversion.flag)
					*ptr++ = conversion.flag;
				if (conversion.width != -1)
					ptr += sprintf(ptr, "%d", conversion.width);
				strcpy(ptr, "Y-%m-%d");

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
				if (!append_string(timeptr->tm_hour < 12 ? nl_langinfo(AM_STR) : nl_langinfo(PM_STR)))
					return 0;
				break;
			case 'r':
				if (size_t ret = strftime(s + len, maxsize - len, nl_langinfo(T_FMT_AMPM), timeptr))
					len += ret;
				else return 0;
				break;
			case 'R':
				if (size_t ret = strftime(s + len, maxsize - len, "%H:%M", timeptr))
					len += ret;
				else return 0;
				break;
			case 's':
			{
				struct tm tm_copy = *timeptr;
				if (!append_value("%llu", mktime(&tm_copy)))
					return 0;
				break;
			}
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

				constexpr auto iso_week_days =
					[](int yday, int wday) -> int
					{
						return yday - (yday - wday + 382) % 7 + 3;
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
						if (conversion.flag == '\0' && 1900 + year > 9999)
							conversion.flag = '+';
						if (conversion.width == -1)
							conversion.width = 4;
						if (!append_value_weird(year, conversion.flag, conversion.width))
							return 0;
						break;
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
				if (size_t ret = strftime(s + len, maxsize - len, nl_langinfo(D_FMT), timeptr))
					len += ret;
				else return 0;
				break;
			case 'X':
				if (size_t ret = strftime(s + len, maxsize - len, nl_langinfo(T_FMT), timeptr))
					len += ret;
				else return 0;
				break;
			case 'y':
				if (!append_value("%02d", timeptr->tm_year % 100))
					return 0;
				break;
			case 'Y':
				if (conversion.flag == '\0' && timeptr->tm_year > 9999 - 1900)
					conversion.flag = '+';
				if (conversion.width == -1)
					conversion.width = 4;
				if (!append_value_weird(1900ll + timeptr->tm_year, conversion.flag, conversion.width))
					return 0;
				break;
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
