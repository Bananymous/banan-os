#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>

int sched_yield(void)
{
	return syscall(SYS_YIELD);
}
