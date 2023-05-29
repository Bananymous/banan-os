#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ERROR(msg) { perror(msg); return 1; }
#define BUF_SIZE 1024

int main()
{
	printf("userspace\n");

	FILE* fp = fopen("/usr/include/kernel/kprint.h", "r");
	if (fp == NULL)
		ERROR("fopen");

	char* buffer = (char*)malloc(128);
	fread(buffer, 1, 100, fp);

	pid_t pid = fork();
	if (pid == 0)
	{
		while (size_t n_read = fread(buffer, 1, 127, fp))
			fwrite(buffer, 1, n_read, stdout);
		free(buffer);
		fclose(fp);
		return 0;
	}

	free(buffer);
	fclose(fp);

	return 0;
}
