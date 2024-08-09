#include <BAN/Assert.h>
#include <BAN/Bitcast.h>

#include <arpa/inet.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int h_errno = 0;

void freeaddrinfo(struct addrinfo*)
{
	ASSERT_NOT_REACHED();
}

const char* gai_strerror(int ecode)
{
	switch (ecode)
	{
		case 0:				return "Success";
		case EAI_AGAIN:		return "The name could not be resolved at this time.";
		case EAI_BADFLAGS:	return "The flags had an invalid value.";
		case EAI_FAIL:		return "A non-recoverable error occurred.";
		case EAI_FAMILY:	return "The address family was not recognized or the address length was invalid for the specified family.";
		case EAI_MEMORY:	return "There was a memory allocation failure.";
		case EAI_NONAME:	return "The name does not resolve for the supplied parameters. ";
		case EAI_SERVICE:	return "The service passed was not recognized for the specified socket type.";
		case EAI_SOCKTYPE:	return "The intended socket type was not recognized.";
		case EAI_SYSTEM:	return "A system error occurred. The error code can be found in errno.";
		case EAI_OVERFLOW:	return "An argument buffer overflowed.";
		default:			return "Unknown error.";
	}
}

int getaddrinfo(const char* __restrict, const char* __restrict, const struct addrinfo* __restrict, struct addrinfo** __restrict)
{
	ASSERT_NOT_REACHED();
}

struct hostent* gethostbyname(const char* name)
{
	static char name_buffer[HOST_NAME_MAX + 1];
	static in_addr_t addr_buffer;
	static in_addr_t* addr_ptrs[2];
	static struct hostent hostent;

	if (strlen(name) > HOST_NAME_MAX)
		return nullptr;
	strcpy(name_buffer, name);
	hostent.h_name = name_buffer;

	hostent.h_aliases = nullptr;

	hostent.h_addrtype = AF_INET;
	hostent.h_length = sizeof(in_addr_t);

	addr_ptrs[0] = &addr_buffer;
	addr_ptrs[1] = nullptr;
	hostent.h_addr_list = reinterpret_cast<char**>(addr_ptrs);

	int socket = -1;

	if (in_addr_t ipv4 = inet_addr(name); ipv4 != (in_addr_t)(-1))
		addr_buffer = ipv4;
	else
	{
		int socket = ::socket(AF_UNIX, SOCK_SEQPACKET, 0);
		if (socket == -1)
			return nullptr;

		sockaddr_un addr;
		addr.sun_family = AF_UNIX;
		strcpy(addr.sun_path, "/tmp/resolver.sock");
		if (connect(socket, (sockaddr*)&addr, sizeof(addr)) == -1)
			goto error_close_socket;

		if (send(socket, name, strlen(name), 0) == -1)
			goto error_close_socket;

		sockaddr_storage storage;
		if (recv(socket, &storage, sizeof(storage), 0) == -1)
			goto error_close_socket;

		close(socket);

		if (storage.ss_family != AF_INET)
			return nullptr;

		addr_buffer = *reinterpret_cast<in_addr_t*>(storage.ss_storage);
	}

	return &hostent;

error_close_socket:
	close(socket);
	return nullptr;
}
