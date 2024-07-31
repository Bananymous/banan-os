#include <BAN/Assert.h>
#include <BAN/Debug.h>

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
    dwarnln("strftime({}, {}, {}, {})", s, maxsize, format, timeptr);
    ASSERT_NOT_REACHED();
}
