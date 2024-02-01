#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

int bind(int socket, const struct sockaddr* address, socklen_t address_len)
{
	return syscall(SYS_BIND, socket, address, address_len);
}

int socket(int domain, int type, int protocol)
{
	return syscall(SYS_SOCKET, domain, type, protocol);
}
