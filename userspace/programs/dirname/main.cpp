#include <libgen.h>
#include <stdio.h>

int usage(const char* argv0, int ret)
{
	FILE* fout = ret ? stderr : stdout;
	fprintf(fout, "usage: %s STRING\n", argv0);
	return ret;
}

int main(int argc, const char* argv[])
{
	if (argc != 2)
		return usage(argv[0], 1);
	const char* result = dirname(const_cast<char*>(argv[1]));
	printf("%s\n", result);
	return 0;
}
