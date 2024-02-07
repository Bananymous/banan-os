#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

int accept(int socket, struct sockaddr* __restrict address, socklen_t* __restrict address_len)
{
	return syscall(SYS_ACCEPT, socket, address, address_len);
}

int bind(int socket, const struct sockaddr* address, socklen_t address_len)
{
	return syscall(SYS_BIND, socket, address, address_len);
}

int connect(int socket, const struct sockaddr* address, socklen_t address_len)
{
	return syscall(SYS_CONNECT, socket, address, address_len);
}

int listen(int socket, int backlog)
{
	return syscall(SYS_LISTEN, socket, backlog);
}

ssize_t recv(int socket, void* __restrict buffer, size_t length, int flags)
{
	return recvfrom(socket, buffer, length, flags, nullptr, nullptr);
}

ssize_t recvfrom(int socket, void* __restrict buffer, size_t length, int flags, struct sockaddr* __restrict address, socklen_t* __restrict address_len)
{
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
	return sendto(socket, message, length, flags, nullptr, 0);
}

ssize_t sendto(int socket, const void* message, size_t length, int flags, const struct sockaddr* dest_addr, socklen_t dest_len)
{
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

int socket(int domain, int type, int protocol)
{
	return syscall(SYS_SOCKET, domain, type, protocol);
}
