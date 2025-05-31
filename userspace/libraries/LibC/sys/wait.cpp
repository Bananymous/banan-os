#include <pthread.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

pid_t wait(int* stat_loc)
{
	pthread_testcancel();
	return waitpid(-1, stat_loc, 0);
}

pid_t waitpid(pid_t pid, int* stat_loc, int options)
{
	pthread_testcancel();
	return (pid_t)syscall(SYS_WAIT, pid, stat_loc, options);
}
