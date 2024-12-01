#ifndef _TIME_H
#define _TIME_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/time.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_clock_t
#define __need_size_t
#define __need_time_t
#define __need_clockid_t
#define __need_timer_t
#define __need_pid_t
#include <sys/types.h>

#define __need_NULL
#include <stddef.h>

#include <bits/types/locale_t.h>

struct sigevent;

struct tm
{
	int tm_sec;		/* Seconds [0,60]. */
	int tm_min;		/* Minutes [0,59]. */
	int tm_hour;	/* Hour [0,23]. */
	int tm_mday;	/* Day of month [1,31]. */
	int tm_mon;		/* Month of year [0,11]. */
	int tm_year;	/* Years since 1900. */
	int tm_wday;	/* Day of week [0,6] (Sunday =0). */
	int tm_yday;	/* Day of year [0,365]. */
	int tm_isdst;	/* Daylight Savings flag. */
};

struct timespec
{
	time_t	tv_sec;		/* Seconds. */
	long	tv_nsec;	/* Nanoseconds. */
};

struct itimerspec
{
	struct timespec it_interval;	/* Timer period. */
	struct timespec it_value;		/* Timer expiration. */
};

#define CLOCKS_PER_SEC ((clock_t)1000000)

#define CLOCK_MONOTONIC				0
#define CLOCK_PROCESS_CPUTIME_ID	1
#define CLOCK_REALTIME				2
#define CLOCK_THREAD_CPUTIME_ID		3

#define TIMER_ABSTIME 1

// FIXME
// #define getdate_err(int)

char*		asctime(const struct tm* timeptr);
char*		asctime_r(const struct tm* __restrict tm, char* __restrict buf);
clock_t		clock(void);
int			clock_getcpuclockid(pid_t pid, clockid_t* clock_id);
int			clock_getres(clockid_t clock_id, struct timespec* res);
int			clock_gettime(clockid_t clock_id, struct timespec* tp);
int			clock_nanosleep(clockid_t clock_id, int flags, const struct timespec* rqtp, struct timespec* rmtp);
int			clock_settime(clockid_t clock_id, const struct timespec* tp);
char*		ctime(const time_t* clock);
char*		ctime_r(const time_t* clock, char* buf);
double		difftime(time_t time1, time_t time0);
struct tm*	getdate(const char* string);
struct tm*	gmtime(const time_t* timer);
struct tm*	gmtime_r(const time_t* __restrict timer, struct tm* __restrict result);
struct tm*	localtime(const time_t* timer);
struct tm*	localtime_r(const time_t* __restrict timer, struct tm* __restrict result);
time_t		mktime(struct tm* timeptr);
int			nanosleep(const struct timespec* rqtp, struct timespec* rmtp);
size_t		strftime(char* __restrict s, size_t maxsize, const char* __restrict format, const struct tm* __restrict timeptr);
size_t		strftime_l(char* __restrict s, size_t maxsize, const char* __restrict format, const struct tm* __restrict timeptr, locale_t locale);
char*		strptime(const char* __restrict buf, const char* __restrict format, struct tm* __restrict tm);
time_t		time(time_t* tloc);
int			timer_create(clockid_t clockid, struct sigevent* __restrict evp, timer_t* __restrict timerid);
int			timer_delete(timer_t timerid);
int			timer_getoverrun(timer_t timerid);
int			timer_gettime(timer_t timerid, struct itimerspec* value);
int			timer_settime(timer_t timerid, int, const struct itimerspec* __restrict value, struct itimerspec* __restrict ovalue);
void		tzset(void);

extern int		daylight;
extern long		timezone;
extern char*	tzname[];

__END_DECLS

#endif
