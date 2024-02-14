#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "usage: %s COMMAND\n", argv[0]);
		return 1;
	}

	FILE* fp = popen(argv[1], "r");
	if (fp == nullptr)
	{
		perror("popen");
		return 1;
	}

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp) != NULL)
		printf("%s", buffer);

	if (pclose(fp) == -1)
	{
		perror("pclose");
		return 1;
	}

	return 0;
}
