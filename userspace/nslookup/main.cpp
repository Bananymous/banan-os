#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

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

	char buffer[128];
	ssize_t nrecv = recv(socket, buffer, sizeof(buffer), 0);
	if (nrecv == -1)
	{
		perror("recv");
		return 1;
	}
	buffer[nrecv] = '\0';

	printf("%s\n", buffer);
	return 0;
}
