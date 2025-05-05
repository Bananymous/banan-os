#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int create_directory(const char* path, bool create_parents)
{
	const size_t pathlen = strlen(path);
	if (pathlen == 0 || pathlen >= PATH_MAX)
	{
		fprintf(stderr, "mkdir: %s\n", strerror(ENOENT));
		return 1;
	}

	if (!create_parents)
	{
		const int ret = mkdir(path, 0755);
		if (ret == -1)
			perror("mkdir");
		return -ret;
	}

	int ret = 0;
	char buffer[PATH_MAX];
	for (size_t i = 0; path[i];)
	{
		for (; path[i] && path[i] != '/'; i++)
			buffer[i] = path[i];
		for (; path[i] && path[i] == '/'; i++)
			buffer[i] = path[i];
		buffer[i] = '\0';
		ret = mkdir(buffer, 0755);
	}

	return ret;
}

int main(int argc, char** argv)
{
	const bool create_parents = argc >= 2 && strcmp(argv[1], "-p") == 0;

	if (argc <= 1 + create_parents)
	{
		fprintf(stderr, "missing operand\n");
		return 1;
	}

	int ret = 0;
	for (int i = 1 + create_parents; i < argc; i++)
		if (create_directory(argv[i], create_parents) == -1)
			ret = 1;

	return ret;
}
