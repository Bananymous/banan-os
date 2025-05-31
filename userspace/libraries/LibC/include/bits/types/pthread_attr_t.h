#ifndef _BITS_TYPES_PTHREAD_ATTR_T_H
#define _BITS_TYPES_PTHREAD_ATTR_T_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/pthread.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <bits/types/sched_param.h>

typedef struct
{
	int inheritsched;
	struct sched_param schedparam;
	int schedpolicy;
	int detachstate;
	int scope;
	size_t stacksize;
	size_t guardsize;
} pthread_attr_t;

__END_DECLS

#endif
