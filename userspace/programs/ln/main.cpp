#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int usage(const char* argv0, int ret)
{
	FILE* fout = ret ? stderr : stdout;
	fprintf(fout, "usage: %s [OPTION]... TARGET [LINK_NAME]\n", argv0);
	fprintf(fout, "  -s, --symbolic   create symbolic link instead of hard link\n");
	fprintf(fout, "  -h, --help       show this message and exit\n");
	return ret;
}

int main(int argc, const char* argv[])
{
	bool do_symlink = false;

	int i = 1;
	for (; i < argc; i++)
	{
		if (argv[i][0] != '-')
			break;

		if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--symbolic") == 0)
			do_symlink = true;
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
			return usage(argv[0], 0);
		else
		{
			fprintf(stderr, "unrecognized option '%s'\n", argv[i]);
			return usage(argv[0], 1);
		}
	}

	if (i == argc)
	{
		fprintf(stderr, "missing target\n");
		return usage(argv[0], 1);
	}

	const char* target = argv[i++];

	struct stat st;
	if (stat(target, &st) == -1)
	{
		perror("stat");
		return 1;
	}

	const char* link_name = (i == argc)
		? basename(const_cast<char*>(target))
		: argv[i];

	int (*link_func)(const char*, const char*) = do_symlink ? &symlink : &link;

	if (link_func(target, link_name) == -1)
	{
		perror(do_symlink ? "symlink" : "link");
		return 1;
	}

	return 0;
}
