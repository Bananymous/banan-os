#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#define SOCK_PATH "/tmp/test.sock"

int server_connection()
{
	int socket = ::socket(AF_UNIX, SOCK_STREAM, 0);
	if (socket == -1)
	{
		perror("server: socket");
		return 1;
	}

	sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCK_PATH);
	if (bind(socket, (sockaddr*)&addr, sizeof(addr)))
	{
		perror("server: bind");
		return 1;
	}

	if (listen(socket, 0) == -1)
	{
		perror("server: listen");
		return 1;
	}

	int client = accept(socket, nullptr, nullptr);
	if (client == -1)
	{
		perror("server: accept");
		return 1;
	}

	sleep(2);

	char buffer[128];
	ssize_t nrecv = recv(client, buffer, sizeof(buffer), 0);
	if (nrecv == -1)
	{
		perror("server: recv");
		return 1;
	}

	printf("server: read %d bytes\n", (int)nrecv);
	printf("server:   '%s'\n", buffer);

	char message[] = "Hello from server";
	if (send(client, message, sizeof(message), 0) == -1)
	{
		perror("server: send");
		return 1;
	}

	close(client);
	close(socket);
	return 0;
}

int client_connection()
{
	sleep(1);

	int socket = ::socket(AF_UNIX, SOCK_STREAM, 0);
	if (socket == -1)
	{
		perror("client: socket");
		return 1;
	}

	sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCK_PATH);
	if (connect(socket, (sockaddr*)&addr, sizeof(addr)) == -1)
	{
		perror("client: connect");
		return 1;
	}

	char message[] = "Hello from client";
	if (send(socket, message, sizeof(message), 0) == -1)
	{
		perror("client: send");
		return 1;
	}

	char buffer[128];
	ssize_t nrecv = recv(socket, buffer, sizeof(buffer), 0);
	if (nrecv == -1)
	{
		perror("client: recv");
		return 1;
	}

	printf("client: read %d bytes\n", (int)nrecv);
	printf("client:   '%s'\n", buffer);

	close(socket);
	return 0;
}

int server_connectionless()
{
	int socket = ::socket(AF_UNIX, SOCK_DGRAM, 0);
	if (socket == -1)
	{
		perror("server: socket");
		return 1;
	}

	sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCK_PATH);
	if (bind(socket, (sockaddr*)&addr, sizeof(addr)))
	{
		perror("server: bind");
		return 1;
	}

	sleep(2);

	char buffer[128];
	ssize_t nrecv = recv(socket, buffer, sizeof(buffer), 0);
	if (nrecv == -1)
	{
		perror("server: recv");
		return 1;
	}

	close(socket);
	return 0;
}

int client_connectionless()
{
	sleep(1);

	int socket = ::socket(AF_UNIX, SOCK_DGRAM, 0);
	if (socket == -1)
	{
		perror("client: socket");
		return 1;
	}

	sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCK_PATH);
	char message[] = "Hello from client";
	if (sendto(socket, message, sizeof(message), 0, (sockaddr*)&addr, sizeof(addr)) == -1)
	{
		perror("client: send");
		return 1;
	}

	close(socket);
	return 0;
}

int test_mode(int (*client)(), int (*server)())
{
	pid_t pid = fork();
	if (pid == -1)
	{
		perror("fork");
		return 1;
	}

	if (pid == 0)
		exit(server());

	if (int ret = client())
	{
		kill(pid, SIGKILL);
		return ret;
	}

	int ret;
	waitpid(pid, &ret, 0);

	if (remove(SOCK_PATH) == -1)
		perror("remove");

	return ret;
}

int main()
{
	if (test_mode(client_connection, server_connection))
		return 1;
	if (test_mode(client_connectionless, server_connectionless))
		return 2;
	return 0;
}
