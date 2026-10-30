#include <pthread.h>
#include <sys/select.h>
#include <sys/syscall.h>
#include <unistd.h>

int pselect(int nfds, fd_set* __restrict readfds, fd_set* __restrict writefds, fd_set* __restrict errorfds, const struct timespec* __restrict timeout, const sigset_t* __restrict sigmask)
{
	pthread_testcancel();
	sys_pselect_t arguments {
		.nfds = nfds,
		.readfds = readfds,
		.writefds = writefds,
		.errorfds = errorfds,
		.timeout = timeout,
		.sigmask = sigmask
	};
	return syscall(SYS_PSELECT, &arguments);
}

int select(int nfds, fd_set* __restrict readfds, fd_set* __restrict writefds, fd_set* __restrict errorfds, struct timeval* __restrict timeout)
{
	if (timeout == nullptr)
		return pselect(nfds, readfds, writefds, errorfds, nullptr, nullptr);
	const timespec timeout_ts {
		.tv_sec  = timeout->tv_sec,
		.tv_nsec = timeout->tv_usec * 1000,
	};
	return pselect(nfds, readfds, writefds, errorfds, &timeout_ts, nullptr);
}
