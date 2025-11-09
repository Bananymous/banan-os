#include <BAN/Debug.h>

#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

int accept(int socket, struct sockaddr* __restrict address, socklen_t* __restrict address_len)
{
	pthread_testcancel();
	return accept4(socket, address, address_len, 0);
}

int accept4(int socket, struct sockaddr* __restrict address, socklen_t* __restrict address_len, int flags)
{
	pthread_testcancel();
	return syscall(SYS_ACCEPT, socket, address, address_len, flags);
}

int bind(int socket, const struct sockaddr* address, socklen_t address_len)
{
	return syscall(SYS_BIND, socket, address, address_len);
}

int connect(int socket, const struct sockaddr* address, socklen_t address_len)
{
	pthread_testcancel();
	return syscall(SYS_CONNECT, socket, address, address_len);
}

int listen(int socket, int backlog)
{
	return syscall(SYS_LISTEN, socket, backlog);
}

ssize_t recv(int socket, void* __restrict buffer, size_t length, int flags)
{
	// cancellation point in recvfrom
	return recvfrom(socket, buffer, length, flags, nullptr, nullptr);
}

ssize_t recvfrom(int socket, void* __restrict buffer, size_t length, int flags, struct sockaddr* __restrict address, socklen_t* __restrict address_len)
{
	// cancellation point in recvmsg

	iovec iov {
		.iov_base = buffer,
		.iov_len = length,
	};

	msghdr message {
		.msg_name = address,
		.msg_namelen = address_len ? *address_len : 0,
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = NULL,
		.msg_controllen = 0,
		.msg_flags = 0,
	};

	const ssize_t ret = recvmsg(socket, &message, flags);

	if (address_len)
		*address_len = message.msg_namelen;

	return ret;
}

ssize_t recvmsg(int socket, struct msghdr* message, int flags)
{
	pthread_testcancel();
	return syscall(SYS_RECVMSG, socket, message, flags);
}

ssize_t send(int socket, const void* buffer, size_t length, int flags)
{
	// cancellation point in sendto
	return sendto(socket, buffer, length, flags, nullptr, 0);
}

ssize_t sendto(int socket, const void* buffer, size_t length, int flags, const struct sockaddr* address, socklen_t address_len)
{
	// cancellation point in sendmsg

	iovec iov {
		.iov_base = const_cast<void*>(buffer),
		.iov_len = length,
	};

	msghdr message {
		.msg_name = const_cast<sockaddr*>(address),
		.msg_namelen = address_len,
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = NULL,
		.msg_controllen = 0,
		.msg_flags = 0,
	};

	return sendmsg(socket, &message, flags);
}

ssize_t sendmsg(int socket, const struct msghdr* message, int flags)
{
	pthread_testcancel();
	return syscall(SYS_SENDMSG, socket, message, flags);
}

int socket(int domain, int type, int protocol)
{
	return syscall(SYS_SOCKET, domain, type, protocol);
}

int socketpair(int domain, int type, int protocol, int socket_vector[2])
{
	return syscall(SYS_SOCKETPAIR, domain, type, protocol, socket_vector);
}

int getsockname(int socket, struct sockaddr* __restrict address, socklen_t* __restrict address_len)
{
	return syscall(SYS_GETSOCKNAME, socket, address, address_len);
}

int getpeername(int socket, struct sockaddr* __restrict address, socklen_t* __restrict address_len)
{
	return syscall(SYS_GETPEERNAME, socket, address, address_len);
}

int getsockopt(int socket, int level, int option_name, void* __restrict option_value, socklen_t* __restrict option_len)
{
	return syscall(SYS_GETSOCKOPT, socket, level, option_name, option_value, option_len);
}

int setsockopt(int socket, int level, int option_name, const void* option_value, socklen_t option_len)
{
	return syscall(SYS_SETSOCKOPT, socket, level, option_name, option_value, option_len);
}

int sockatmark(int s)
{
	dwarnln("TODO: sockatmark({})", s);
	errno = ENOTSUP;
	return -1;
}

int shutdown(int socket, int how)
{
	dwarnln("TODO: shutdown({}, {})", socket, how);
	errno = ENOTSUP;
	return -1;
}
