#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

pid_t wait(int* stat_loc)
{
	return waitpid(-1, stat_loc, 0);
}

pid_t waitpid(pid_t pid, int* stat_loc, int options)
{
	return (pid_t)syscall(SYS_WAIT, pid, stat_loc, options);
}
