#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

int raise(int sig)
{
	return syscall(SYS_RAISE, sig);
}

int kill(pid_t pid, int sig)
{
	return syscall(SYS_KILL, pid, sig);
}