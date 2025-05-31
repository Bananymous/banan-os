#ifndef _BITS_TYPES_TIMEVAL_H
#define _BITS_TYPES_TIMEVAL_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_time_t
#define __need_suseconds_t
#include <sys/types.h>

struct timeval
{
	time_t		tv_sec;		/* Seconds. */
	suseconds_t	tv_usec;	/* Microseconds. */
};

__END_DECLS

#endif
