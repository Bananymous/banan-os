#ifndef _SCHED_H
#define _SCHED_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sched.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <time.h>

#define __need_pid_t
#include <sys/types.h>

struct sched_param
{
	int				sched_priority;			/* Process or thread execution scheduling priority. */
	int				sched_ss_low_priority;  /* Low scheduling priority for sporadic server. */
	struct timespec	sched_ss_repl_period;	/* Replenishment period for sporadic server. */
	struct timespec	sched_ss_init_budget;	/* Initial budget for sporadic server. */
	int				sched_ss_max_repl;		/* Maximum pending replenishments for sporadic server. */
};

#define SCHED_FIFO		1
#define SCHED_RR		2
#define SCHED_SPORADIC	3
#define SCHED_OTHER		4

int sched_get_priority_max(int policy);
int sched_get_priority_min(int policy);
int sched_getparam(pid_t pid, struct sched_param* param);
int sched_getscheduler(pid_t pid);
int sched_rr_get_interval(pid_t pid, struct timespec* interval);
int sched_setparam(pid_t pid, const struct sched_param* param);
int sched_setscheduler(pid_t pid, int, const struct sched_param* param);
int sched_yield(void);

__END_DECLS

#endif
