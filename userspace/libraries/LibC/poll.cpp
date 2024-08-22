#include <poll.h>
#include <sys/select.h>

int poll(struct pollfd fds[], nfds_t nfds, int timeout)
{
	fd_set rfds, wfds, efds;
	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&efds);

	for (nfds_t i = 0; i < nfds; i++)
		fds[i].revents = 0;

	constexpr short rmask = POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI | POLLHUP;
	constexpr short wmask = POLLOUT | POLLWRNORM | POLLWRBAND;
	constexpr short emask = POLLERR;

	int max_fd = 0;
	for (nfds_t i = 0; i < nfds; i++)
	{
		if (fds[i].fd < 0)
			continue;

		if (fds[i].events & rmask)
			FD_SET(fds[i].fd, &rfds);
		if (fds[i].events & wmask)
			FD_SET(fds[i].fd, &wfds);
		if (fds[i].events & emask)
			FD_SET(fds[i].fd, &efds);

		if (fds[i].fd > max_fd)
			max_fd = fds[i].fd;
	}

	timeval tv;
	tv.tv_sec = timeout / 1000;
	tv.tv_usec = timeout % 1000 * 1000;
	int nselect = select(max_fd + 1, &rfds, &wfds, &efds, &tv);
	if (nselect == -1)
		return -1;

	for (nfds_t i = 0; i < nfds; i++)
	{
		if (fds[i].fd < 0)
			continue;

		if (FD_ISSET(fds[i].fd, &rfds))
			fds[i].revents |= fds[i].events & rmask;
		if (FD_ISSET(fds[i].fd, &wfds))
			fds[i].revents |= fds[i].events & wmask;
		if (FD_ISSET(fds[i].fd, &efds))
			fds[i].revents |= fds[i].events & emask;
	}

	return nselect;
}
