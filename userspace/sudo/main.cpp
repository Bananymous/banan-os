#include <stdio.h>
#include <unistd.h>

int usage(char* argv0, int ret)
{
	FILE* fp = (ret == 0) ? stdout : stderr;
	fprintf(fp, "usage: %s COMMAND [ARGUMENTS...]\n", argv0);
	return ret;
}

int main(int argc, char** argv)
{
	if (argc < 2)
		return usage(argv[0], 1);

	if (setuid(0) == -1)
	{
		perror("setuid");
		return 1;
	}
	if (setgid(0) == -1)
	{
		perror("setgid");
		return 1;
	}

	execvp(argv[1], argv + 1);

	perror("execvp");
	return 1;
}
