#ifndef _SYS_TIME_H
#define _SYS_TIME_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_time.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

// NOTE: select is declared from here
#include <sys/select.h>

#include <bits/types/timeval.h>

struct itimerval
{
	struct timeval it_interval;	/* Timer interval. */
	struct timeval it_value;	/* Current value. */
};

#define ITIMER_REAL		0
#define ITIMER_VIRTUAL	1
#define ITIMER_PROF		2

struct timezone
{
	int tz_minuteswest;	/* minutes west of Greenwich */
	int tz_dsttime;		/* type of DST correction */
};

int getitimer(int which, struct itimerval* value);
int gettimeofday(struct timeval* __restrict tp, void* __restrict tzp);
int setitimer(int which, const struct itimerval* __restrict value, struct itimerval* __restrict ovalue);
int utimes(const char* path, const struct timeval times[2]);

__END_DECLS

#endif
