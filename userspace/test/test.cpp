#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define ERROR(msg) { perror(msg); return 1; }
#define BUF_SIZE 1024

int main()
{
	printf("%.2e\n", 1230.0);
	printf("%.2e\n", 123.0);
	printf("%.2e\n", 12.3);
	printf("%.2e\n", 1.23);
	printf("%.2e\n", 0.123);
	printf("%.2e\n", 0.0123);
	printf("%.2e\n", 0.00123);

	printf("%e\n", 123.456);
	printf("%.2e\n", 123.456);
	printf("%.0e\n", 123.456);
	printf("%#.0e\n", 123.456);

	printf("%e\n", -123.456);
	printf("%.2e\n", -123.456);
	printf("%.0e\n", -123.456);
	printf("%#.0e\n", -123.456);

	printf("%e\n", 0.0);
	printf("%e\n", -0.0);

	return 0;

	FILE* fp = fopen("/usr/include/stdio.h", "r");
	if (fp == NULL)
		ERROR("fopen");

	char* buffer = (char*)malloc(BUF_SIZE);
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
