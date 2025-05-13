#ifndef _SYS_EPOLL_H
#define _SYS_EPOLL_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <stdint.h>
#include <signal.h>

union epoll_data
{
	void* ptr;
	int fd;
	uint32_t u32;
	uint64_t u64;
};
typedef union epoll_data epoll_data_t;

struct epoll_event
{
	uint32_t events;
	epoll_data_t data;
};

#define EPOLL_CTL_ADD 0
#define EPOLL_CTL_MOD 1
#define EPOLL_CTL_DEL 2

#define EPOLLIN      0x01
#define EPOLLOUT     0x02
#define EPOLLPRI     0x04
#define EPOLLERR     0x08
#define EPOLLHUP     0x10
#define EPOLLET      0x20
#define EPOLLONESHOT 0x40

#define EPOLL_CLOEXEC 1

int epoll_create(int size);
int epoll_create1(int flags);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event);
int epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout);
int epoll_pwait(int epfd, struct epoll_event* events, int maxevents, int timeout, const sigset_t* sigmask);
int epoll_pwait2(int epfd, struct epoll_event* events, int maxevents, const struct timespec* timeout, const sigset_t* sigmask);

__END_DECLS

#endif
