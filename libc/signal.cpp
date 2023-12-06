#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

int raise(int sig)
{
	// FIXME: won't work after multithreaded
	return kill(getpid(), sig);
}

int kill(pid_t pid, int sig)
{
	return syscall(SYS_KILL, pid, sig);
}

void (*signal(int sig, void (*func)(int)))(int)
{
	long ret = syscall(SYS_SIGNAL, sig, func);
	return (void (*)(int))ret;
}