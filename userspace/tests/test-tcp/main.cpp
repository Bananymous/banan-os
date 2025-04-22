#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

in_addr_t get_ipv4_address(const char* query)
{
	const addrinfo hints {
		.ai_flags = 0,
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = 0,
		.ai_addrlen = 0,
		.ai_addr = nullptr,
		.ai_canonname = nullptr,
		.ai_next = nullptr,
	};

	addrinfo* result;
	if (getaddrinfo(query, nullptr, &hints, &result) != 0)
		return -1;

	for (addrinfo* ai = result; ai; ai = ai->ai_next)
		if (ai->ai_family != AF_INET)
			return reinterpret_cast<sockaddr_in*>(ai->ai_addr)->sin_addr.s_addr;

	return -1;
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "usage: %s IPADDR\n", argv[0]);
		return 1;
	}

	in_addr_t ipv4 = get_ipv4_address(argv[1]);
	if (ipv4 == (in_addr_t)(-1))
	{
		fprintf(stderr, "could not parse address '%s'\n", argv[1]);
		return 1;
	}

	int socket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (socket == -1)
	{
		perror("socket");
		return 1;
	}

	printf("connecting to %s\n", inet_ntoa({ .s_addr = ipv4 }));

	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(80);
	server_addr.sin_addr.s_addr = ipv4;
	if (connect(socket, (sockaddr*)&server_addr, sizeof(server_addr)) == -1)
	{
		perror("connect");
		return 1;
	}

	char request[128];
	strcpy(request, "GET / HTTP/1.1\r\n");
	strcat(request, "Host: "); strcat(request, argv[1]); strcat(request, "\r\n");
	strcat(request, "Accept: */*\r\n");
	strcat(request, "Connection: close\r\n");
	strcat(request, "\r\n");
	if (send(socket, request, strlen(request), 0) == -1)
	{
		perror("send");
		return 1;
	}

	char buffer[1024];
	for (;;)
	{
		ssize_t nrecv = recv(socket, buffer, sizeof(buffer), 0);
		if (nrecv == -1)
			perror("recv");
		if (nrecv <= 0)
			break;
		write(STDOUT_FILENO, buffer, nrecv);
	}

	close(socket);
	return 0;
}
