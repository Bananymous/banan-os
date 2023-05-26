#ifndef _SYS_TIMES_H
#define _SYS_TIMES_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_times.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_clock_t
#include <sys/types.h>

struct tms
{
	clock_t tms_utime;	/* User CPU time. */
	clock_t tms_stime;	/* System CPU time. */
	clock_t tms_cutime;	/* User CPU time of terminated child processes. */
	clock_t tms_cstime;	/* System CPU time of terminated child processes. */
};

clock_t times(struct tms* buffer);

__END_DECLS

#endif
