#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

bool all { false };
bool list { false };

const char* mode_string(mode_t mode)
{
	static char buffer[11];
	buffer[0] =	(mode & 0770000) == S_IFLNK ? 'l' :
				(mode & 0770000) == S_IFDIR ? 'd' :
				(mode & 0770000) == S_IFBLK ? 'b' :
				(mode & 0770000) == S_IFCHR ? 'c' :
				'-';
	buffer[1] = mode & S_IRUSR ? 'r' : '-';
	buffer[2] = mode & S_IWUSR ? 'w' : '-';
	buffer[3] = mode & S_IXUSR ? 'x' : '-';
	buffer[4] = mode & S_IRGRP ? 'r' : '-';
	buffer[5] = mode & S_IWGRP ? 'w' : '-';
	buffer[6] = mode & S_IXGRP ? 'x' : '-';
	buffer[7] = mode & S_IROTH ? 'r' : '-';
	buffer[8] = mode & S_IWOTH ? 'w' : '-';
	buffer[9] = mode & S_IXOTH ? 'x' : '-';
	buffer[10] = '\0';

	return buffer;
}

void list_directory(const char* path)
{
	DIR* dirp = opendir(path);
	if (dirp == nullptr)
		return perror("opendir");
	
	errno = 0;
	
	bool first = true;
	while (auto* dirent = readdir(dirp))
	{
		if (!all && dirent->d_name[0] == '.')
			continue;

		if (list)
		{
			if (!first)
				printf("\n");

			struct stat st;
			if (fstatat(dirfd(dirp), dirent->d_name, &st, AT_SYMLINK_NOFOLLOW) == -1)
			{
				perror("stat");
				printf("?????????? ???? %s", dirent->d_name);
			}
			else
			{
				printf("%s %d %s", mode_string(st.st_mode), st.st_size, dirent->d_name);
			}
		}
		else
		{
			if (!first)
				printf(" ");
			printf("%s", dirent->d_name);
		}

		first = false;
	}

	if (errno != 0)
		perror("readdir");

	printf("\n");

	closedir(dirp);
}

void print_usage()
{
	printf("Usage: ls [OPTION]... [FILE]...\n");
	printf("  -a, --all     show all files\n");
	printf("  -l, --list    print files on separate lines\n");
}

int main(int argc, char** argv)
{
	int arg = 1;

	while (arg < argc && argv[arg][0] == '-')
	{
		if (strcmp(argv[arg], "--all") == 0)
			all = true;
		else if (strcmp(argv[arg], "--list") == 0)
			list = true;
		else if (strcmp(argv[arg], "--help") == 0)
		{
			print_usage();
			return 0;
		}
		else
		{
			for (int i = 1; argv[arg][i]; i++)
			{
				char c = argv[arg][i];
				if (c == 'a')
					all = true;
				else if (c == 'l')
					list = true;
				else
				{
					print_usage();
					return 1;
				}
			}
		}

		arg++;
	}

	if (arg == argc)
	{
		list_directory(".");
	}
	else if (arg + 1 == argc)
	{
		list_directory(argv[arg]);
	}
	else
	{
		for (int i = arg; i < argc; i++)
		{
			if (i > arg)
				printf("\n");
			printf("%s:\n", argv[i]);
			list_directory(argv[i]);
		}
	}

	return 0;
}
