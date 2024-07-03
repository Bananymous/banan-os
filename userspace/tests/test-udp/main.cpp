#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int usage(const char* argv0)
{
	fprintf(stderr, "usage: %s [-s|-c] [-a addr] [-p port]\n", argv0);
	return 1;
}

int main(int argc, char** argv)
{
	bool server = false;
	uint32_t addr = 0;
	uint16_t port = 0;

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-s") == 0)
			server = true;
		else if (strcmp(argv[i], "-c") == 0)
			server = false;
		else if (strcmp(argv[i], "-a") == 0)
			addr = inet_addr(argv[++i]);
		else if (strcmp(argv[i], "-p") == 0)
			sscanf(argv[++i], "%hu", &port);
		else
			return usage(argv[0]);
	}

	int socket = ::socket(AF_INET, SOCK_DGRAM, 0);
	if (socket == -1)
	{
		perror("socket");
		return 1;
	}

	if (server)
	{
		sockaddr_in bind_addr;
		bind_addr.sin_family = AF_INET;
		bind_addr.sin_port = htons(port);
		bind_addr.sin_addr.s_addr = addr;

		if (bind(socket, (sockaddr*)&bind_addr, sizeof(bind_addr)) == -1)
		{
			perror("bind");
			return 1;
		}

		printf("listening on %s:%hu\n", inet_ntoa(bind_addr.sin_addr), ntohs(bind_addr.sin_port));

		char buffer[1024];
		sockaddr_in sender;
		socklen_t sender_len = sizeof(sender);

		if (recvfrom(socket, buffer, sizeof(buffer), 0, (sockaddr*)&sender, &sender_len) == -1)
		{
			perror("recvfrom");
			return 1;
		}

		printf("received from %s:%hu\n", inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));
		printf("  %s\n", buffer);
	}
	else
	{
		const char buffer[] = "Hello from banan-os!";

		sockaddr_in server_addr;
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(port);
		server_addr.sin_addr.s_addr = addr;

		printf("sending to %s:%hu\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));
		if (sendto(socket, buffer, sizeof(buffer), 0, (sockaddr*)&server_addr, sizeof(server_addr)) == -1)
		{
			perror("sendto");
			return 1;
		}
	}

	close(socket);
	return 0;
}
