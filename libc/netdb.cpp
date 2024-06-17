#include <BAN/Bitcast.h>

#include <arpa/inet.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

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
