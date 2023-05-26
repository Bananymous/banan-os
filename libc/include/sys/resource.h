#ifndef _SYS_RESOURCE_H
#define _SYS_RESOURCE_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_resource.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_id_t
#include <sys/types.h>

#include <sys/time.h>

#define PRIO_PROCESS	0
#define PRIO_PGRP		1
#define PRIO_USER		2

typedef unsigned int rlim_t;

#define RLIM_INFINITY	((rlim_t)-1)
#define RLIM_SAVED_MAX	RLIM_INFINITY
#define RLIM_SAVED_CUR	RLIM_INFINITY

#define RUSAGE_SELF		0
#define RUSAGE_CHILDREN 1

struct rlimit
{
	rlim_t rlim_cur;	/* The current (soft) limit. */
	rlim_t rlim_max;	/* The hard limit. */
};

struct rusage
{
	struct timeval ru_utime;	/* User time used. */
	struct timeval ru_stime;	/* System time used. */	
};

#define RLIMIT_CORE		0
#define RLIMIT_CPU		1
#define RLIMIT_DATA		2
#define RLIMIT_FSIZE	3
#define RLIMIT_NOFILE	4
#define RLIMIT_STACK	5
#define RLIMIT_AS		6

int getpriority(int which, id_t who);
int getrlimit(int resource, struct rlimit* rlp);
int getrusage(int who, struct rusage* r_usage);
int setpriority(int which, id_t who, int value);
int setrlimit(int resource, const struct rlimit* rlp);

__END_DECLS

#endif
