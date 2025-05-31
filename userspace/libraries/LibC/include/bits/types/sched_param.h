#ifndef _BITS_TYPES_SCHED_PARAM_H
#define _BITS_TYPES_SCHED_PARAM_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sched.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <time.h>

struct sched_param
{
	int				sched_priority;			/* Process or thread execution scheduling priority. */
	int				sched_ss_low_priority;  /* Low scheduling priority for sporadic server. */
	struct timespec	sched_ss_repl_period;	/* Replenishment period for sporadic server. */
	struct timespec	sched_ss_init_budget;	/* Initial budget for sporadic server. */
	int				sched_ss_max_repl;		/* Maximum pending replenishments for sporadic server. */
};

__END_DECLS

#endif
