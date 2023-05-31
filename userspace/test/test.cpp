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
		execl("/usr/bin/cat", "/usr/bin/cat", "/usr/include/kernel/kprint.h", NULL);
		ERROR("execl");
		return 0;
	}

	printf("parent\n");
	return 0;
}
