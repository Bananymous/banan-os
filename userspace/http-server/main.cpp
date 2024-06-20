#include <BAN/Vector.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

int main()
{
	int socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socket == -1)
	{
		perror("socket");
		return 1;
	}

	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(socket, (sockaddr*)&addr, sizeof(addr)) == -1)
	{
		perror("bind");
		return 1;
	}

	if (listen(socket, SOMAXCONN) == -1)
	{
		perror("listen");
		return 1;
	}

	printf("server started\n");

	BAN::Vector<int> clients;

	char buffer[1024];
	while (true)
	{
		int max_sock = socket;

		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(socket, &fds);
		for (int client : clients)
		{
			FD_SET(client, &fds);
			max_sock = BAN::Math::max(client, max_sock);
		}

		if (select(max_sock + 1, &fds, nullptr, nullptr, nullptr) == -1)
		{
			perror("select");
			break;
		}

		if (FD_ISSET(socket, &fds))
		{
			int client = accept(socket, nullptr, nullptr);
			if (client == -1)
			{
				perror("accept");
				continue;
			}

			printf("client %d connected\n", client);

			MUST(clients.push_back(client));
		}

		for (size_t i = 0; i < clients.size();)
		{
			if (!FD_ISSET(clients[i], &fds))
			{
				i++;
				continue;
			}

			ssize_t nrecv = recv(clients[i], buffer, sizeof(buffer), 0);
			if (nrecv < 0)
				perror("recv");
			if (nrecv <= 0)
			{
				printf("%d disconnected\n", clients[i]);
				close(clients[i]);
				clients.remove(i);
				continue;
			}

			write(STDOUT_FILENO, buffer, nrecv);

			strcpy(buffer, "HTTP/1.1 200 OK\r\nContent-Length: 13\r\nConnection: close\r\n\r\nHello, world!");
			ssize_t nsend = send(clients[i], buffer, strlen(buffer), 0);
			if (nsend < 0)
				perror("send");
			if (nsend <= 0)
			{
				printf("%d disconnected\n", clients[i]);
				close(clients[i]);
				clients.remove(i);
				continue;
			}

			i++;
		}
	}
}
