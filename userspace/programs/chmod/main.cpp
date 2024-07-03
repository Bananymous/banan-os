#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

void usage(const char* argv0, int ret)
{
	FILE* out = (ret == 0) ? stdout : stderr;
	fprintf(out, "usage: %s MODE FILE...\n", argv0);
	fprintf(out, "  Change the mode of each FILE to MODE.\n");
	exit(ret);
}

int main(int argc, char** argv)
{
	if (argc <= 2)
		usage(argv[0], 1);

	int base = (argv[1][0] == '0') ? 8 : 10;

	mode_t mode = 0;
	for (const char* ptr = argv[1]; *ptr; ptr++)
	{
		if (!isdigit(*ptr))
		{
			fprintf(stderr, "Invalid MODE %s\n", argv[1]);
			usage(argv[0], 1);
		}
		mode = (mode * base) + (*ptr - '0');
	}

	int ret = 0;
	for (int i = 2; i < argc; i++)
	{
		if (chmod(argv[i], mode) == -1)
		{
			perror("chmod");
			ret = 1;
		}
	}

	return ret;
}
