#include <stdio.h>
#include <stdlib.h>

int main()
{
	void* ptr = malloc(10);
	if (ptr == NULL)
	{
		perror("malloc");
		return 1;
	}
	*(int*)ptr = 5;
	putc('0' + *(int*)ptr, stdout);
	return 0;

	FILE* fp = fopen("/boot/grub/grub.cfg", "r");
	if (fp == NULL)
	{
		perror("fopen");
		return 1;
	}

	for (;;)
	{
		char buffer[128];
		size_t nread = fread(buffer, 1, sizeof(buffer) - 1, fp);
		if (nread == 0)
		{
			if (ferror(fp))
				perror("fread");
			break;
		}
		buffer[nread] = '\0';
		fputs(buffer, stdout);
	}

	if (fclose(fp) == EOF)
	{
		perror("fclose");
		return 1;
	}

	return 0;
}
