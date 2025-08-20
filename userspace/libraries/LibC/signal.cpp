#include <BAN/Assert.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

static_assert(sizeof(sigset_t) * 8 >= _SIGMAX);

static int validate_signal(int sig)
{
	if (_SIGMIN <= sig && sig <= _SIGMAX)
		return 0;
	errno = EINVAL;
	return -1;
}

int kill(pid_t pid, int sig)
{
	return syscall(SYS_KILL, pid, sig);
}

int killpg(pid_t pgrp, int sig)
{
	if (pgrp <= 1)
	{
		errno = EINVAL;
		return -1;
	}
	return kill(-pgrp, sig);
}

void psignal(int signum, const char* message)
{
	if (message && *message)
		fprintf(stderr, "%s: %s\n", message, strsignal(signum));
	else
		fprintf(stderr, "%s\n", strsignal(signum));
}

int pthread_kill(pthread_t thread, int sig)
{
	if (syscall(SYS_PTHREAD_KILL, thread, sig) == -1)
		return errno;
	return 0;
}

int pthread_sigmask(int how, const sigset_t* __restrict set, sigset_t* __restrict oset)
{
	if (syscall(SYS_SIGPROCMASK, how, set, oset) == -1)
		return errno;
	return 0;
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
	if (validate_signal(signo) == -1)
		return -1;
	*set |= 1ull << signo;
	return 0;
}

int sigdelset(sigset_t* set, int signo)
{
	if (validate_signal(signo) == -1)
		return -1;
	*set &= ~(1ull << signo);
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

int sighold(int sig)
{
	if (validate_signal(sig) == -1)
		return -1;
	sigset_t set;
	(void)sigemptyset(&set);
	(void)sigaddset(&set, sig);
	return sigprocmask(SIG_BLOCK, &set, nullptr);
}

int sigignore(int sig)
{
	if (signal(sig, SIG_IGN) == SIG_ERR)
		return -1;
	return 0;
}

int siginterrupt(int sig, int flag)
{
	if (validate_signal(sig) == -1)
		return -1;
    struct sigaction act;
    (void)sigaction(sig, nullptr, &act);
    if (flag)
        act.sa_flags &= ~SA_RESTART;
    else
        act.sa_flags |= SA_RESTART;
    return sigaction(sig, &act, nullptr);
}

int sigismember(const sigset_t* set, int signo)
{
	if (validate_signal(signo) == -1)
		return -1;
	return (*set >> signo) & 1;
}

void (*signal(int sig, void (*func)(int)))(int)
{
	struct sigaction act, oact;
	act.sa_handler = func;
	act.sa_flags = 0;

	int ret = sigaction(sig, &act, &oact);
	if (ret == -1)
		return SIG_ERR;
	return oact.sa_handler;
}

int sigpending(sigset_t* set)
{
	return syscall(SYS_SIGPENDING, set);
}

int sigprocmask(int how, const sigset_t* __restrict set, sigset_t* __restrict oset)
{
	if (int error = pthread_sigmask(how, set, oset))
	{
		errno = error;
		return -1;
	}
	return 0;
}

int sigrelse(int sig)
{
	if (validate_signal(sig) == -1)
		return -1;
	sigset_t set;
	(void)sigemptyset(&set);
	(void)sigaddset(&set, sig);
	return sigprocmask(SIG_UNBLOCK, &set, nullptr);
}

int sigsuspend(const sigset_t* sigmask)
{
	return syscall(SYS_SIGSUSPEND, sigmask);
}
