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
	pthread_testcancel();
	sys_recvfrom_t arguments {
		.socket = socket,
		.buffer = buffer,
		.length = length,
		.flags = flags,
		.address = address,
		.address_len = address_len
	};
	return syscall(SYS_RECVFROM, &arguments);
}

ssize_t send(int socket, const void* message, size_t length, int flags)
{
	// cancellation point in sendto
	return sendto(socket, message, length, flags, nullptr, 0);
}

ssize_t sendto(int socket, const void* message, size_t length, int flags, const struct sockaddr* dest_addr, socklen_t dest_len)
{
	pthread_testcancel();
	sys_sendto_t arguments {
		.socket = socket,
		.message = message,
		.length = length,
		.flags = flags,
		.dest_addr = dest_addr,
		.dest_len = dest_len
	};
	return syscall(SYS_SENDTO, &arguments);
}

ssize_t	recvmsg(int socket, struct msghdr* message, int flags)
{
	if (CMSG_FIRSTHDR(message))
	{
		dwarnln("TODO: recvmsg ancillary data");
		errno = ENOTSUP;
		return -1;
	}

	size_t total_recv = 0;

	for (int i = 0; i < message->msg_iovlen; i++)
	{
		const ssize_t nrecv = recvfrom(
			socket,
			message->msg_iov[i].iov_base,
			message->msg_iov[i].iov_len,
			flags,
			static_cast<sockaddr*>(message->msg_name),
			&message->msg_namelen
		);

		if (nrecv < 0)
			return -1;

		total_recv += nrecv;

		if (static_cast<size_t>(nrecv) < message->msg_iov[i].iov_len)
			break;
	}

	return total_recv;
}

ssize_t	sendmsg(int socket, const struct msghdr* message, int flags)
{
	if (CMSG_FIRSTHDR(message))
	{
		dwarnln("TODO: sendmsg ancillary data");
		errno = ENOTSUP;
		return -1;
	}

	size_t total_sent = 0;

	for (int i = 0; i < message->msg_iovlen; i++)
	{
		const ssize_t nsend = sendto(
			socket,
			message->msg_iov[i].iov_base,
			message->msg_iov[i].iov_len,
			flags,
			static_cast<sockaddr*>(message->msg_name),
			message->msg_namelen
		);

		if (nsend < 0)
			return -1;

		total_sent += nsend;

		if (static_cast<size_t>(nsend) < message->msg_iov[i].iov_len)
			break;
	}

	return total_sent;
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



#include <BAN/Assert.h>

int shutdown(int, int)
{
	ASSERT_NOT_REACHED();
}
