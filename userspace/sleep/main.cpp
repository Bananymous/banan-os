#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int usage(char* argv0, int ret)
{
	FILE* fp = (ret == 0) ? stdout : stderr;
	fprintf(fp, "usage: %s SECONDS\n", argv0);
	return ret;
}

int main(int argc, char** argv)
{
	if (argc != 2)
		return usage(argv[0], 1);
	
	if (strlen(argv[1]) > 9)
	{
		fprintf(stderr, "SECONDS argument too large\n");
		return usage(argv[0], 1);
	}

	int seconds = 0;
	const char* ptr = argv[1];
	while (*ptr)
	{
		if (!isdigit(*ptr))
		{
			fprintf(stderr, "invalid SECONDS argument\n");
			return usage(argv[0], 1);
		}
		seconds = (seconds * 10) + (*ptr - '0');
		ptr++;
	}

	sleep(seconds);
}
