#pragma once

#include <locale.h>
#include <sys/types.h>

__BEGIN_DECLS

struct tm
{
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

struct timespec
{
	time_t tv_sec;
	long tv_nsec;
};

char*		asctime(const struct tm*);
char*		asctime_r(const struct tm*, char*);
clock_t		clock(void);
int			clock_getcpuclockid(pid_t, clockid_t*);
int			clock_getres(clockid_t, struct timespe*);
int			clock_gettime(clockid_t, struct timespec*);
int			clock_nanosleep(clockid_t, int, const struct timespec*, struct timespec*);
int			clock_settime(clockid_t, const struct timespec*);
char*		ctime(const time_t*);
char*		ctime_r(const time_t*, char*);
double		difftime(time_t, time_t);
struct tm*	getdate(const char*);
struct tm*	gmtime(const time_t*);
struct tm*	gmtime_r(const time_t*, struct tm*);
struct tm*	localtime(const time_t*);
struct tm*	localtime_r(const time_t*, struct tm*);
time_t		mktime(struct tm*);
int			nanosleep(const struct timespec*, struct timespec*);
size_t		strftime(char*, size_t, const char*, const struct tm*);
size_t		strftime_l(char*, size_t, const char*, const struct tm*, locale_t);
char		*strptime(const char*, const char*, struct tm*);
time_t		time(time_t*);
int			timer_create(clockid_t, struct sigevent*, timer_t*);
int			timer_delete(timer_t);
int			timer_getoverrun(timer_t);
int			timer_gettime(timer_t, struct itimerspec*);
int			timer_settime(timer_t, int, const struct itimerspec*, struct itimerspec*);
void		tzset(void);

__END_DECLS