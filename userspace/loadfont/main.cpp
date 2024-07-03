#include <stdio.h>
#include <stropts.h>
#include <unistd.h>

int usage(int ret, const char* argv0)
{
	FILE* fout = ret ? stderr : stdout;
	fprintf(fout, "usage: %s FILE\n", argv0);
	return ret;
}

int main(int argc, char** argv)
{
	if (argc != 2)
		return usage(1, argv[0]);

	if (!isatty(STDOUT_FILENO))
	{
		fprintf(stderr, "stdout is not tty\n");
		return 1;
	}

	if (ioctl(STDOUT_FILENO, KD_LOADFONT, argv[1]) == -1)
	{
		perror("ioctl");
		return 1;
	}

	return 0;
}
