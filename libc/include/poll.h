#ifndef _POLL_H
#define _POLL_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/poll.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

struct pollfd
{
	int		fd;			/* The following descriptor being polled. */
	short	events;		/* The input event flags. */
	short	revents;	/* The output event flags. */
};

typedef unsigned long nfds_t;

#define POLLIN		0x001
#define POLLRDNORM	0x002
#define POLLRDBAND	0x004
#define POLLPRI		0x008
#define POLLOUT		0x010
#define POLLWRNORM	0x020
#define POLLWRBAND	0x040
#define POLLERR		0x080
#define POLLHUP		0x100
#define POLLNVAL	0x200

int poll(struct pollfd fds[], nfds_t nfds, int timeout);

__END_DECLS

#endif
