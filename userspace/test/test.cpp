#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ERROR(msg) { perror(msg); return 1; }
#define BUF_SIZE 1024

int main()
{
	printf("userspace\n");

	if (fork() == 0)
	{
		char* argv[3];
		argv[0] = (char*)malloc(100); strcpy(argv[0], "/usr/bin/cat");
		argv[1] = (char*)malloc(100); strcpy(argv[1], "/usr/include/kernel/kprint.h");
		argv[2] = NULL;
		execv("/usr/bin/cat", (char**)argv);
		ERROR("execl");
		return 0;
	}

	printf("parent\n");
	return 0;
}
