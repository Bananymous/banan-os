#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_socket.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_size_t
#define __need_ssize_t
#include <sys/types.h>

#include <sys/uio.h>

#include <bits/types/sa_family_t.h>
#include <bits/types/socklen_t.h>

#if !defined(FILENAME_MAX)
	#define FILENAME_MAX 256
#elif FILENAME_MAX != 256
	#error "invalid FILENAME_MAX"
#endif

struct sockaddr
{
	sa_family_t	sa_family;	/* Address family. */
	char		sa_data[0];	/* Socket address (variable-length data). */
};

struct sockaddr_storage
{
	sa_family_t ss_family;
	char		ss_storage[FILENAME_MAX];
};

struct msghdr
{
	void*			msg_name;		/* Optional address. */
	socklen_t		msg_namelen;	/* Size of address. */
	struct iovec*	msg_iov;		/* Scatter/gather array. */
	int				msg_iovlen;		/* Members in msg_iov. */
	void*			msg_control;	/* Ancillary data; see below. */
	socklen_t		msg_controllen;	/* Ancillary data buffer len. */
	int				msg_flags;		/* Flags on received message. */
};

struct cmsghdr
{
	socklen_t	cmsg_len;	/* Data byte count, including the cmsghdr. */
	int			cmsg_level;	/* Originating protocol. */
	int			cmsg_type;	/* Protocol-specific type. */
	unsigned char __cmg_data[];
};

#define SCM_RIGHTS 1

#define CMSG_DATA(cmsg) ((cmsg)->__cmg_data)

#define __CMSG_NXTHDR_ADDR(cmsg) \
	((unsigned char*)(cmsg) + (cmsg)->cmsg_len)
#define __CMSG_NXTHDR_OFFSET(mhdr, cmsg) \
	(__CMSG_NXTHDR_ADDR(cmsg) - (unsigned char*)(mhdr)->msg_control)
#define CMSG_NXTHDR(mhdr, cmsg) \
	((size_t)(mhdr)->msg_controllen \
			>= __CMSG_NXTHDR_OFFSET(mhdr, cmsg) + sizeof(struct cmsghdr) \
		? (struct cmsghdr*)__CMSG_NXTHDR_ADDR(cmsg) \
		: (struct cmsghdr*)0)

#define CMSG_FIRSTHDR(mhdr) \
	((size_t)(mhdr)->msg_controllen >= sizeof(struct cmsghdr) \
		? (struct cmsghdr*)((mhdr)->msg_control) \
		: (struct cmsghdr*)0)

#define CMSG_SPACE(length) \
	(socklen_t)((length) + sizeof(struct cmsghdr))

#define CMSG_LEN(length) \
	(socklen_t)((length) + sizeof(struct cmsghdr))

struct linger
{
	int l_onoff;	/* Indicates wheter linger option is enabled. */
	int l_linger;	/* Linger time, in seconds. */
};

#define SOCK_DGRAM		1
#define SOCK_RAW		2
#define SOCK_SEQPACKET	3
#define SOCK_STREAM		4
#define SOCK_CLOEXEC	0x10
#define SOCK_NONBLOCK	0x20

#define SOL_SOCKET 1

#define SO_ACCEPTCONN	0
#define SO_BROADCAST	1
#define SO_DEBUG		2
#define SO_DONTROUTE	3
#define SO_ERROR		4
#define SO_KEEPALIVE	5
#define SO_LINGER		6
#define SO_OOBINLINE	7
#define SO_RCVBUF		8
#define SO_RCVLOWAT		9
#define SO_RCVTIMEO		10
#define SO_REUSEADDR	11
#define SO_SNDBUF		12
#define SO_SNDLOWAT		13
#define SO_SNDTIMEO		14
#define SO_TYPE			15

#define SOMAXCONN 4096

#define MSG_CTRUNC		0x01
#define MSG_DONTROUTE	0x02
#define MSG_EOR			0x04
#define MSG_OOB			0x08
#define MSG_NOSIGNAL	0x10
#define MSG_PEEK		0x20
#define MSG_TRUNC		0x40
#define MSG_WAITALL		0x80

#define AF_UNSPEC	0
#define AF_INET		1
#define AF_INET6	2
#define AF_UNIX		3

#define PF_UNSPEC	AF_UNSPEC
#define PF_INET		AF_INET
#define PF_INET6	AF_INET6
#define PF_UNIX		AF_UNIX

#define SHUT_RD		0x01
#define SHUT_WR		0x02
#define SHUT_RDWR	(SHUT_RD | SHUT_WR)

struct sys_sendto_t
{
	int socket;
	const void* message;
	size_t length;
	int flags;
	const struct sockaddr* dest_addr;
	socklen_t dest_len;
};

struct sys_recvfrom_t
{
	int socket;
	void* buffer;
	size_t length;
	int flags;
	struct sockaddr* address;
	socklen_t* address_len;
};

int		accept(int socket, struct sockaddr* __restrict address, socklen_t* __restrict address_len);
int		accept4(int socket, struct sockaddr* __restrict address, socklen_t* __restrict address_len, int flags);
int		bind(int socket, const struct sockaddr* address, socklen_t address_len);
int		connect(int socket, const struct sockaddr* address, socklen_t address_len);
int		getpeername(int socket, struct sockaddr* __restrict address, socklen_t* __restrict address_len);
int		getsockname(int socket, struct sockaddr* __restrict address, socklen_t* __restrict address_len);
int		getsockopt(int socket, int level, int option_name, void* __restrict option_value, socklen_t* __restrict option_len);
int		listen(int socket, int backlog);
ssize_t	recv(int socket, void* buffer, size_t length, int flags);
ssize_t	recvfrom(int socket, void* __restrict buffer, size_t length, int flags, struct sockaddr* __restrict address, socklen_t* __restrict address_len);
ssize_t	recvmsg(int socket, struct msghdr* message, int flags);
ssize_t	send(int socket, const void* buffer, size_t length, int flags);
ssize_t	sendmsg(int socket, const struct msghdr* message, int flags);
ssize_t	sendto(int socket, const void* message, size_t length, int flags, const struct sockaddr* dest_addr, socklen_t dest_len);
int		setsockopt(int socket, int level, int option_name, const void* option_value, socklen_t option_len);
int		shutdown(int socket, int how);
int		sockatmark(int s);
int		socket(int domain, int type, int protocol);
int		socketpair(int domain, int type, int protocol, int socket_vector[2]);

__END_DECLS

#endif
