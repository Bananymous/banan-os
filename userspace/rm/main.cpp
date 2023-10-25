#include <stdio.h>
#include <unistd.h>

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
		if (unlink(argv[i]) == -1)
		{
			perror(argv[i]);
			ret = 1;
		}
	}
	return ret;
}
