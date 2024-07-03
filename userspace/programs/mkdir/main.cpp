#include <stdio.h>
#include <sys/stat.h>

int main(int argc, char** argv)
{
	if (argc <= 1)
	{
		fprintf(stderr, "Missing operand\n");
		return 1;
	}

	int ret = 0;
	for (int i = 1; i < argc; i++)
	{
		if (mkdir(argv[i], 0755) == -1)
		{
			perror("mkdir");
			ret = 1;
		}
	}

	return ret;
}
