#include <stdio.h>
#include <stdlib.h>

#define ERROR(msg) { perror(msg); return 1; }
#define BUF_SIZE 1024

int main()
{
	FILE* fp = fopen("/usr/include/stdio.h", "r");
	if (fp == NULL)
		ERROR("fopen");

	char* buffer = malloc(BUF_SIZE);
	if (buffer == NULL)
		ERROR("malloc");

	for (;;)
	{
		size_t n_read = fread(buffer, 1, BUF_SIZE - 1, fp);
		if (n_read == 0)
			break;
		buffer[n_read] = '\0';
		printf("%s", buffer);
	}

	free(buffer);
	fclose(fp);

	return 0;
}
