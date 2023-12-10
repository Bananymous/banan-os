#include <BAN/String.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool delete_recursive(const char* path)
{
	struct stat st;
	if (stat(path, &st) == -1)
	{
		perror(path);
		return false;
	}

	bool ret = true;

	if (S_ISDIR(st.st_mode))
	{
		DIR* dir = opendir(path);
		while (struct dirent* dirent = readdir(dir))
		{
			if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
				continue;

			BAN::String dirent_path;
			MUST(dirent_path.append(path));
			MUST(dirent_path.push_back('/'));
			MUST(dirent_path.append(dirent->d_name));

			if (dirent->d_type == DT_DIR)
			{
				if (!delete_recursive(dirent_path.data()))
					ret = false;
			}
			else
			{
				if (unlink(dirent_path.data()) == -1)
				{
					perror(dirent_path.data());
					ret = false;
				}
			}
		}

		closedir(dir);
	}

	if (unlink(path) == -1)
	{
		perror(path);
		return false;
	}

	return ret;
}

void usage(const char* argv0, int ret)
{
	FILE* out = (ret == 0) ? stdout : stderr;
	fprintf(out, "usage: %s [OPTIONS]... FILE...\n", argv0);
	fprintf(out, "  remove each FILE\n");
	fprintf(out, "OPTIONS:\n");
	fprintf(out, "  -r           remove directories and their contents recursively\n");
	fprintf(out, "  -h, --help   show this message and exit\n");
	exit(ret);
}

int main(int argc, char** argv)
{
	bool recursive = false;

	int i = 1;
	for (; i < argc; i++)
	{
		if (argv[i][0] != '-')
			break;

		if (strcmp(argv[i], "-r") == 0)
			recursive = true;
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
			usage(argv[0], 0);
		else
		{
			fprintf(stderr, "unrecognized argument %s. use --help for more information\n", argv[i]);
			return 1;
		}
	}

	if (i >= argc)
	{
		fprintf(stderr, "missing operand. use --help for more information\n");
		return 1;
	}

	int ret = 0;
	for (; i < argc; i++)
	{
		if (recursive)
		{
			if (!delete_recursive(argv[i]))
				ret = 1;
		}
		else
		{
			struct stat st;
			if (stat(argv[i], &st) == -1)
			{
				perror(argv[i]);
				ret = 1;
				continue;
			}
			
			if (S_ISDIR(st.st_mode))
			{
				fprintf(stderr, "%s: %s\n", argv[i], strerror(EISDIR));
				ret = 1;
				continue;
			}

			if (unlink(argv[i]) == -1)
			{
				perror(argv[i]);
				ret = 1;
			}
		}
	}
	return ret;
}
