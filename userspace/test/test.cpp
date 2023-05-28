#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ERROR(msg) { perror(msg); return 1; }
#define BUF_SIZE 1024

int main()
{
	char* string = (char*)malloc(5000);
	strcpy(string, "Hello");

	printf("forking\n");

	pid_t pid = fork();
	if (pid == 0)
	{
		printf("child '%s'\n", string);
		return 0;
	}
	printf("parent '%s'\n", string);

	return 0;
}
