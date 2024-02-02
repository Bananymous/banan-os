#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

int bind(int socket, const struct sockaddr* address, socklen_t address_len)
{
	return syscall(SYS_BIND, socket, address, address_len);
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
