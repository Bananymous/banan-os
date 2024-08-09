#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_select.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <bits/types/timeval.h>

#include <signal.h>
#include <time.h>

#define FD_SETSIZE 1024

typedef unsigned long __fd_mask;
#define __FD_MASK_SIZE (8 * sizeof(__fd_mask))

typedef struct {
	__fd_mask __fds_bits[FD_SETSIZE / __FD_MASK_SIZE];
} fd_set;

#define FD_CLR(fd, setp) \
	do { \
		__fd_mask off = (fd) / __FD_MASK_SIZE; \
		__fd_mask bit = (fd) % __FD_MASK_SIZE; \
		(setp)->__fds_bits[off] &= ~((__fd_mask)1 << bit); \
	} while (0)

#define FD_ISSET(fd, setp) \
	({ \
		__fd_mask off = (fd) / __FD_MASK_SIZE; \
		__fd_mask bit = (fd) % __FD_MASK_SIZE; \
		(setp)->__fds_bits[off] & ((__fd_mask)1 << bit); \
	})

#define FD_SET(fd, setp) \
	do { \
		__fd_mask off = (fd) / __FD_MASK_SIZE; \
		__fd_mask bit = (fd) % __FD_MASK_SIZE; \
		(setp)->__fds_bits[off] |= ((__fd_mask)1 << bit); \
	} while (0)

#define FD_ZERO(setp) \
	do { \
		for (int i = 0; i < (int)FD_SETSIZE / (int)__FD_MASK_SIZE; i++) \
			(setp)->__fds_bits[i] = (__fd_mask)0; \
	} while (0)

struct sys_pselect_t
{
	int nfds;
	fd_set* readfds;
	fd_set* writefds;
	fd_set* errorfds;
	const struct timespec* timeout;
	const sigset_t* sigmask;
};

int pselect(int nfds, fd_set* __restrict readfds, fd_set* __restrict writefds, fd_set* __restrict errorfds, const struct timespec* __restrict timeout, const sigset_t* __restrict sigmask);
int select(int nfds, fd_set* __restrict readfds, fd_set* __restrict writefds, fd_set* __restrict errorfds, struct timeval* __restrict timeout);

__END_DECLS

#endif
