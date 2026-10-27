#include <BAN/Atomic.h>
#include <BAN/Assert.h>
#include <BAN/Debug.h>
#include <BAN/Math.h>

#include <kernel/API/SharedPage.h>

#include <bits/strftime.hpp>
#include <ctype.h>
#include <errno.h>
#include <langinfo.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

int daylight;
long timezone;
char* tzname[2];

extern volatile Kernel::API::SharedPage* g_shared_page;

int clock_gettime(clockid_t clock_id, struct timespec* tp)
{
	if (clock_id != CLOCK_MONOTONIC && clock_id != CLOCK_REALTIME)
		return syscall(SYS_CLOCK_GETTIME, clock_id, tp);

	if (g_shared_page == nullptr || !(g_shared_page->features & Kernel::API::SPF_GETTIME))
		return syscall(SYS_CLOCK_GETTIME, clock_id, tp);

	uint32_t mult;
	int8_t   shift;
	uint64_t last_ns;
	uint64_t last_tsc;
	uint64_t curr_tsc;

	const auto read_tsc_info = [&](volatile decltype(g_shared_page->cpus[0].gettime_local)& lgettime) {
		uint32_t seq1, seq2;
		do {
			seq1     = BAN::atomic_load(lgettime.seq, BAN::memory_order_acquire);
			mult     = lgettime.mult;
			shift    = lgettime.shift;
			last_ns  = lgettime.last_ns;
			last_tsc = lgettime.last_tsc;
			seq2     = BAN::atomic_load(lgettime.seq, BAN::memory_order_acquire);
		} while (seq1 != seq2 || (seq1 & 1));
	};

	if (g_shared_page->features & Kernel::API::SPF_RDTSCP)
	{
		do {
			uint32_t cpu;
			curr_tsc = __builtin_ia32_rdtscp(&cpu);
			read_tsc_info(g_shared_page->cpus[cpu].gettime_local);
		} while (curr_tsc < last_tsc);
	}
	else
	{
		uint16_t cpu1, cpu2;
		do {
			asm volatile("lsl %1, %0" : "=r"(cpu1) : "r"(g_shared_page->gdt_cpu_offset));
			read_tsc_info(g_shared_page->cpus[cpu1].gettime_local);
			curr_tsc = __builtin_ia32_rdtsc();
			asm volatile("lsl %1, %0" : "=r"(cpu2) : "r"(g_shared_page->gdt_cpu_offset));
		} while (cpu1 != cpu2);
	}

	uint64_t clock_ns = curr_tsc - last_tsc;
	if (shift >= 0)
		clock_ns <<= shift;
	else
		clock_ns >>= -shift;
	clock_ns = (clock_ns * mult) >> 32;
	clock_ns += last_ns;

	if (clock_id == CLOCK_REALTIME)
	{
		const auto& sgettime = g_shared_page->gettime_shared;
		clock_ns += sgettime.realtime_s * 1'000'000'000 + sgettime.realtime_ns;
	}

	*tp = {
		.tv_sec = static_cast<time_t>(clock_ns / 1'000'000'000),
		.tv_nsec = static_cast<long>(clock_ns % 1'000'000'000)
	};

	return 0;
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
	tzset();

	struct tm modified = *tm;

	if (modified.tm_year < 70)
	{
		errno = EOVERFLOW;
		return -1;
	}

	modified.tm_min += modified.tm_sec / 60;
	modified.tm_sec %= 60;

	modified.tm_hour += modified.tm_min / 60;
	modified.tm_min %= 60;

	modified.tm_mday += modified.tm_hour / 24;
	modified.tm_hour %= 24;

	static constexpr int month_days[] { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	for (;;)
	{
		int days_in_month = month_days[modified.tm_mon];
		if (modified.tm_mon == 1 && is_leap_year(modified.tm_year))
			days_in_month++;

		if (modified.tm_mday <= days_in_month)
			break;

		modified.tm_mday -= days_in_month;
		modified.tm_mon++;
	}

	modified.tm_year += modified.tm_mon / 12;
	modified.tm_mon %= 12;

	modified.tm_yday = modified.tm_mday - 1;
	for (int i = 0; i < modified.tm_mon; i++)
		modified.tm_yday += month_days[i];

	const time_t num_febs = (modified.tm_mon > 1) ? modified.tm_year + 1 : modified.tm_year;
	const time_t leap_years = (num_febs - 69) / 4 - (num_febs - 1) / 100 + (num_febs + 299) / 400;

	const time_t years = modified.tm_year - 70;
	const time_t days = years * 365 + leap_years + modified.tm_yday;
	const time_t hours = days * 24 + modified.tm_hour;
	const time_t minutes = hours * 60 + modified.tm_min;
	const time_t seconds = minutes * 60 + modified.tm_sec;

	*tm = modified;
	tm->tm_wday = (days + 4) % 7;
	tm->tm_isdst = daylight;

	return seconds;
}

struct tm* gmtime_r(const time_t* timer, struct tm* __restrict result)
{
	// FIXME: allow negative times :)
	ASSERT(*timer >= 0);

	constexpr uint64_t month_days[] { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };

	unsigned long long time = *timer;

	result->tm_sec  = time % 60; time /= 60;
	result->tm_min  = time % 60; time /= 60;
	result->tm_hour = time % 24; time /= 24;

	unsigned long long total_days = time;
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
	return _strftime(s, maxsize, format, timeptr);
}

static int parse_tm_name(const char* buf, const char** out, int ab, int full, size_t count) {
    int tm_name = -1;
    for(size_t i = 0; i < count; ++i) {
        const char *smol = nl_langinfo(ab + i),
                   *big = nl_langinfo(full + i);

        size_t smol_len = strlen(smol),
               big_len = strlen(big);
        if(strncasecmp(buf, big, big_len) == 0) {
            buf += big_len;
            tm_name = i;
            break;
        }
        if(strncasecmp(buf, smol, smol_len) == 0) {
            buf += smol_len;
            tm_name = i;
            break;
        }
    }
    *out = buf;
    return tm_name;
}

char* strptime(const char* buf, const char* format, struct tm* tm) {
    for(;;) {
        while(*format && *format != '%') {
            if(isspace(*format)) {
                while(isspace(*buf)) buf++;
                format++;
            } else if(*buf++ != *format++) return NULL;
        }
        if(*format == '\0') break;
        format++;
        char fmt_chr = *format++;
        switch(fmt_chr) {
        case '%': if(*buf != '%') return NULL; break;
        case 'a': case 'A': {
            int day = parse_tm_name(buf, &buf, ABDAY_1, DAY_1, 7);
            if(day == -1) return NULL;
            tm->tm_wday = day;
        } break;
        case 'b': case 'B': case 'h': {
            int mon = parse_tm_name(buf, &buf, ABMON_1, MON_1, 12);
            if(mon == -1) return NULL;
            tm->tm_mon = mon;
        } break;
        case 'd': case 'e': {
            errno = 0;
            long day = strtol(buf, (char**)&buf, 10);
            if(errno) return NULL;
            if(day < 1 || day > 31) return NULL;
            tm->tm_mday = day;
        } break;
        case 'Y': {
            errno = 0;
            long year = strtol(buf, (char**)&buf, 10);
            if(errno) return NULL;
            tm->tm_year = year - 1900;
        } break;
        case 'H': {
            errno = 0;
            long hour = strtol(buf, (char**)&buf, 10);
            if(errno) return NULL;
            if(hour < 0 || hour > 23) return NULL;
            tm->tm_hour = hour;
        } break;
        case 'M': {
            errno = 0;
            long minute = strtol(buf, (char**)&buf, 10);
            if(errno) return NULL;
            if(minute < 0 || minute > 59) return NULL;
            tm->tm_min = minute;
        } break;
        case 'S': {
            errno = 0;
            long sec = strtol(buf, (char**)&buf, 10);
            if(errno) return NULL;
            if(sec < 0 || sec > 60) return NULL;
            tm->tm_sec = sec;
        } break;
        case 'D': {
            buf = strptime(buf, "%m/%d/%y", tm);
            if (buf == NULL) return NULL;
        } break;
        case 'R': {
            buf = strptime(buf, "%H:%M", tm);
            if (buf == NULL) return NULL;
        } break;
        case 'T': {
            buf = strptime(buf, "%H:%M:%S", tm);
            if (buf == NULL) return NULL;
        } break;
        default:
            derrorln("TODO: strptime fmt {}", fmt_chr);
            ASSERT_NOT_REACHED();
        }
    }
    return (char*)buf;
}
