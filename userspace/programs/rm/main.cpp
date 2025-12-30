#include <BAN/String.h>

#include <dirent.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool prompt_removal(const char* path, struct stat st)
{
	const char* type = "file";
	if (S_ISREG(st.st_mode))
		type = st.st_size ? "regular file" : "regular empty file";
	else if (S_ISDIR(st.st_mode))
		type = "directory";
	else if (S_ISCHR(st.st_mode))
		type = "character special file";
	else if (S_ISBLK(st.st_mode))
		type = "block special file";
	else if (S_ISLNK(st.st_mode))
		type = "symbolic link";
	else if (S_ISFIFO(st.st_mode))
		type = "fifo";

	fprintf(stderr, "remove %s '%s'? ", type, path);

	char buffer[128];
	if (fgets(buffer, sizeof(buffer), stdin) == nullptr)
		return false;
	return buffer[0] == 'Y' || buffer[0] == 'y';
}

bool delete_recursive(const char* path, bool force, bool interactive)
{
	struct stat st;
	if (stat(path, &st) == -1)
	{
		if (force && errno == ENOENT)
			return true;
		perror(path);
		return false;
	}

	if (interactive && !prompt_removal(path, st))
		return true;

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
				if (!delete_recursive(dirent_path.data(), force, interactive))
					ret = false;
			}
			else
			{
				if (interactive && !prompt_removal(dirent_path.data(), st))
					continue;
				if (unlink(dirent_path.data()) == -1)
				{
					perror(dirent_path.data());
					ret = false;
				}
			}
		}

		closedir(dir);
	}

	if (remove(path) == -1)
	{
		perror(path);
		return false;
	}

	return ret;
}

int main(int argc, char** argv)
{
	bool force = false;
	bool interactive = false;
	bool recursive = false;

	for (;;)
	{
		static option long_options[] {
			{ "recursive",   no_argument, nullptr, 'r' },
			{ "interactive", no_argument, nullptr, 'i' },
			{ "force",       no_argument, nullptr, 'f' },
			{ "help",        no_argument, nullptr, 'h' },
		};

		int ch = getopt_long(argc, argv, "rRifh", long_options, nullptr);
		if (ch == -1)
			break;

		switch (ch)
		{
			case 'h':
				fprintf(stderr, "usage: %s [OPTIONS]... FILE...\n", argv[0]);
				fprintf(stderr, "  remove each FILE\n");
				fprintf(stderr, "OPTIONS:\n");
				fprintf(stderr, "  -r, -R, --recursive  remove directories and their contents recursively\n");
				fprintf(stderr, "  -i, --interactive    prompt removal for all files\n");
				fprintf(stderr, "  -f, --force          ignore nonexistent files and never prompt\n");
				fprintf(stderr, "  -h, --help           show this message and exit\n");
				return 0;
			case 'r': case 'R':
				recursive = true;
				break;
			case 'f':
				force = true;
				interactive = false;
				break;
			case 'i':
				force = false;
				interactive = true;
				break;
			case '?':
				fprintf(stderr, "invalid option %c\n", optopt);
				fprintf(stderr, "see '%s --help' for usage\n", argv[0]);
				return 1;
		}
	}

	if (optind >= argc && !force)
	{
		fprintf(stderr, "missing operand. use --help for more information\n");
		return 1;
	}

	int ret = 0;
	for (int i = optind; i < argc; i++)
	{
		if (recursive)
		{
			if (!delete_recursive(argv[i], force, interactive))
				ret = 1;
			continue;
		}

		struct stat st;
		if (stat(argv[i], &st) == -1)
		{
			if (force && errno == ENOENT)
				continue;
			perror(argv[i]);
			ret = 1;
			continue;
		}

		if (interactive && !prompt_removal(argv[i], st))
			continue;

		if (S_ISDIR(st.st_mode))
		{
			errno = EISDIR;
			perror(argv[i]);
			ret = 1;
			continue;
		}

		if (unlink(argv[i]) == -1)
		{
			perror(argv[i]);
			ret = 1;
		}
	}

	return ret;
}
