#include <poll.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>

int poll(struct pollfd fds[], nfds_t nfds, int timeout)
{
	pthread_testcancel();
	if (timeout < 0)
		return ppoll(fds, nfds, nullptr, nullptr);
	const timespec timeout_ts {
		.tv_sec = static_cast<time_t>(timeout / 1000),
		.tv_nsec = static_cast<long>(timeout % 1000),
	};
	return ppoll(fds, nfds, &timeout_ts, nullptr);
}

int ppoll(struct pollfd fds[], nfds_t nfds, const struct timespec* timeout, const sigset_t* sigmask)
{
	pthread_testcancel();
	return syscall(SYS_PPOLL, fds, nfds, timeout, sigmask);
}
