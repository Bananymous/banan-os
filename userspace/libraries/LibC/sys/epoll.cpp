#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/syscall.h>

int epoll_create(int size)
{
	if (size <= 0)
	{
		errno = EINVAL;
		return -1;
	}

	return epoll_create1(0);
}

int epoll_create1(int flags)
{
	return syscall(SYS_EPOLL_CREATE1, flags);
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event)
{
	return syscall(SYS_EPOLL_CTL, epfd, op, fd, event);
}

int epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout)
{
	return epoll_pwait(epfd, events, maxevents, timeout, nullptr);
}

int epoll_pwait(int epfd, struct epoll_event* events, int maxevents, int timeout, const sigset_t* sigmask)
{
	timespec ts;
	timespec* ts_ptr = nullptr;
	if (timeout >= 0)
	{
		ts.tv_sec = static_cast<time_t>(timeout / 1000),
		ts.tv_nsec = (timeout % 1000) * 1'000'000,
		ts_ptr = &ts;
	}

	return epoll_pwait2(epfd, events, maxevents, ts_ptr, sigmask);
}

int epoll_pwait2(int epfd, struct epoll_event* events, int maxevents, const struct timespec* timeout, const sigset_t* sigmask)
{
	return syscall(SYS_EPOLL_PWAIT2, epfd, events, maxevents, timeout, sigmask);
}
