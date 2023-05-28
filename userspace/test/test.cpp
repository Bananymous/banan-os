#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ERROR(msg) { perror(msg); return 1; }
#define BUF_SIZE 1024

int main()
{
	printf("forking\n");

	pid_t pid = fork();
	if (pid == 0)
	{
		printf("child\n");
		return 0;
	}
	printf("parent\n");

	return 0;
}
