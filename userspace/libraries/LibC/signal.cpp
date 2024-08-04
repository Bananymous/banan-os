#include <BAN/Assert.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

static_assert(sizeof(sigset_t) * 8 >= _SIGMAX);

int kill(pid_t pid, int sig)
{
	return syscall(SYS_KILL, pid, sig);
}

void psignal(int signum, const char* message)
{
	if (message && *message)
		fprintf(stderr, "%s: %s\n", message, strsignal(signum));
	else
		fprintf(stderr, "%s\n", strsignal(signum));
}

int raise(int sig)
{
	// FIXME: won't work after multithreaded
	return kill(getpid(), sig);
}

int sigaction(int sig, const struct sigaction* __restrict act, struct sigaction* __restrict oact)
{
	return syscall(SYS_SIGACTION, sig, act, oact);
}

int sigaddset(sigset_t* set, int signo)
{
	*set |= 1ull << signo;
	return 0;
}

int sigemptyset(sigset_t* set)
{
	*set = 0;
	return 0;
}

int sigfillset(sigset_t* set)
{
	*set = (1ull << _SIGMAX) - 1;
	return 0;
}

int sigismember(const sigset_t* set, int signo)
{
	return (*set >> signo) & 1;
}

void (*signal(int sig, void (*func)(int)))(int)
{
	struct sigaction act;
	act.sa_handler = func;
	act.sa_flags = 0;

	int ret = sigaction(sig, &act, nullptr);
	if (ret == -1)
		return SIG_ERR;
	return func;
}

int sigpending(sigset_t* set)
{
	return syscall(SYS_SIGPENDING, set);
}

int sigprocmask(int how, const sigset_t* __restrict set, sigset_t* __restrict oset)
{
	return syscall(SYS_SIGPROCMASK, how, set, oset);
}
