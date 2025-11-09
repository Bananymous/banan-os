#ifndef _POLL_H
#define _POLL_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/poll.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <signal.h>

struct pollfd
{
	int		fd;			/* The following descriptor being polled. */
	short	events;		/* The input event flags. */
	short	revents;	/* The output event flags. */
};

typedef unsigned long nfds_t;

#define POLLIN		0x001
#define POLLOUT		0x002
#define POLLERR		0x004
#define POLLHUP		0x008
#define POLLPRI		0x010
#define POLLRDNORM	0x020
#define POLLRDBAND	0x040
#define POLLWRNORM	0x080
#define POLLWRBAND	0x100
#define POLLNVAL	0x200

int poll(struct pollfd fds[], nfds_t nfds, int timeout);
int ppoll(struct pollfd fds[], nfds_t nfds, const struct timespec* timeout, const sigset_t* sigmask);

__END_DECLS

#endif
