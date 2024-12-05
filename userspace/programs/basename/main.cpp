#include <libgen.h>
#include <stdio.h>
#include <string.h>

int usage(const char* argv0, int ret)
{
	FILE* fout = ret ? stderr : stdout;
	fprintf(fout, "usage: %s STRING [SUFFIX]\n", argv0);
	return ret;
}

int main(int argc, const char* argv[])
{
	if (argc != 2 && argc != 3)
		return usage(argv[0], 1);

	const char* result = basename(const_cast<char*>(argv[1]));
	int result_len = strlen(result);

	if (argc == 3)
	{
		int suffix_len = strlen(argv[2]);
		if (result_len >= suffix_len && strcmp(result - suffix_len, argv[2]) == 0)
			result_len -= suffix_len;
	}

	printf("%.*s\n", result_len, result);

	return 0;
}
