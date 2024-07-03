#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX(a, b) ((a) < (b) ? (b) : (a))

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "usage: %s DOMAIN\n", argv[0]);
		return 1;
	}

	int socket = ::socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (socket == -1)
	{
		perror("socket");
		return 1;
	}

	sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/tmp/resolver.sock");
	if (connect(socket, (sockaddr*)&addr, sizeof(addr)) == -1)
	{
		perror("connect");
		return 1;
	}

	if (send(socket, argv[1], strlen(argv[1]), 0) == -1)
	{
		perror("send");
		return 1;
	}

	sockaddr_storage storage;
	if (recv(socket, &storage, sizeof(storage), 0) == -1)
	{
		perror("recv");
		return 1;
	}

	close(socket);

	char buffer[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)];
	printf("%s\n", inet_ntop(storage.ss_family, storage.ss_storage, buffer, sizeof(buffer)));

	return 0;
}
