#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>

int sched_get_priority_max(int policy)
{
	(void)policy;
	return 0;
}

int sched_get_priority_min(int policy)
{
	(void)policy;
	return 0;
}

int sched_yield(void)
{
	return syscall(SYS_YIELD);
}
