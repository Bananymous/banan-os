#ifndef _SYS_TIME_H
#define _SYS_TIME_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_time.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_time_t
#define __need_suseconds_t
#include <sys/types.h>

#include <sys/select.h>

struct timeval
{
	time_t		tv_sec;		/* Seconds. */
	suseconds_t	tc_usec;	/* Microseconds. */
};

struct itimerval
{
	struct timeval it_interval;	/* Timer interval. */
	struct timeval it_value;	/* Current value. */
};

#define ITIMER_REAL		0
#define ITIMER_VIRTUAL	1
#define ITIMER_PROF		2

int getitimer(int which, struct itimerval* value);
int gettimeofday(struct timeval* __restrict tp, void* __restrict tzp);
int setitimer(int which, const struct itimerval* __restrict value, struct itimerval* __restrict ovalue);
int select(int nfds, fd_set* __restrict readfds, fd_set* __restrict writefds, fd_set* __restrict errorfds, struct timeval* __restrict timeout);
int utimes(const char* path, const struct timeval times[2]);

__END_DECLS

#endif
