#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int copy_file(const char* src, const char* dst)
{
	struct stat src_st;
	if (stat(src, &src_st) == -1)
		return errno;

	struct stat dst_st;
	if (stat(dst, &dst_st) == 0)
	{
		if (S_ISDIR(dst_st.st_mode))
			return EINVAL;
		if (unlinkat(AT_FDCWD, dst, 0) == -1)
			return errno;
	}

	if (S_ISREG(src_st.st_mode))
	{
		const int src_fd = open(src, O_RDONLY);
		const int dst_fd = open(dst, O_RDWR | O_CREAT | O_EXCL, src_st.st_mode);

		if (src_fd == -1 || dst_fd == -1)
		{
			if (src_fd != -1)
				close(src_fd);
			if (dst_fd != -1)
				close(dst_fd);
			return errno;
		}

		int result = 0;

		char buffer[512];
		for (;;)
		{
			const ssize_t nread = read(src_fd, buffer, 512);
			if (nread <= 0)
			{
				if (nread == -1)
					result = errno;
				break;
			}

			ssize_t total_written = 0;
			while (total_written < nread)
			{
				const ssize_t nwrite = write(dst_fd, buffer + total_written, nread - total_written);
				if (nwrite < 0)
				{
					result = errno;
					break;
				}
				total_written += nwrite;
			}
		}

		if (result == 0 && unlink(src) == -1)
			result = errno;

		close(src_fd);
		close(dst_fd);
		return result;
	}

	if (S_ISLNK(src_st.st_mode))
	{
		char* buffer = static_cast<char*>(malloc(512));
		if (buffer == nullptr)
			return errno;
		ssize_t buffer_size = 512;

		ssize_t link_len;
		while ((link_len = readlink(src, buffer, buffer_size)) == buffer_size)
		{
			buffer_size *= 2;
			void* new_buffer = realloc(buffer, buffer_size);
			if (new_buffer == nullptr)
			{
				free(buffer);
				return errno;
			}
			buffer = static_cast<char*>(new_buffer);
		}

		int result = 0;
		if (link_len == -1)
			result = errno;
		if (result == 0 && symlink(buffer, dst) == -1)
			result = errno;
		if (result == 0 && unlink(src) == -1)
			result = errno;
		free(buffer);
		return result;
	}

	fprintf(stddbg, "move file with mode %07o to another filesystem\n", src_st.st_mode);
	return ENOTSUP;
}

static int copy_directory(const char* src, const char* dst)
{
	struct stat src_st;
	if (stat(src, &src_st) == -1)
		return errno;

	struct stat dst_st;
	if (stat(dst, &dst_st) == 0)
	{
		if (!S_ISDIR(dst_st.st_mode))
			return ENOTDIR;
		if (rmdir(dst) == -1)
			return errno;
	}

	if (mkdir(dst, src_st.st_mode) == -1)
		return errno;

	DIR* dirp = opendir(src);
	if (dirp == nullptr)
		return errno;

	int result = 0;

	dirent* dent;
	while ((dent = readdir(dirp)))
	{
		if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
			continue;

		bool name_too_long = false;

		char src_buffer[PATH_MAX];
		if (snprintf(src_buffer, PATH_MAX, "%s/%s", src, dent->d_name) > PATH_MAX)
			name_too_long = true;

		char dst_buffer[PATH_MAX];
		if (snprintf(dst_buffer, PATH_MAX, "%s/%s", dst, dent->d_name) > PATH_MAX)
			name_too_long = true;

		if (name_too_long)
			result = ENAMETOOLONG;
		else
		{
			auto* copy_func = (dent->d_type == DT_DIR) ? copy_directory : copy_file;
			if (int ret = copy_func(src_buffer, dst_buffer))
				result = ret;
		}
	}

	if (result == 0 && rmdir(src) == -1)
		result = errno;

	closedir(dirp);

	return result;
}

static int do_move(const char* src, const char* dest)
{
	char buffer[PATH_MAX];

	struct stat st;
	if (stat(dest, &st) == 0 && S_ISDIR(st.st_mode))
	{
		if (snprintf(buffer, PATH_MAX, "%s/%s", dest, basename(const_cast<char*>(src))) > PATH_MAX)
			return ENAMETOOLONG;
		dest = buffer;
	}

	if (rename(src, dest) == 0)
		return 0;

	if (errno != EXDEV)
		return errno;

	if (stat(src, &st) == -1)
		return errno;

	auto* copy_func = S_ISDIR(st.st_mode) ? copy_directory : copy_file;
	return copy_func(src, dest);
}

int main(int argc, char** argv)
{
	for (;;)
	{
		static option long_options[] {
			{ "help", no_argument, nullptr, 'h' },
		};

		int ch = getopt_long(argc, argv, "h", long_options, nullptr);
		if (ch == -1)
			break;

		switch (ch)
		{
			case 'h':
				printf("usage: %s [OPTIONS]... SOURCE... DEST\n", argv[0]);
				printf("Moves files SOURCE... to DEST\n");
				printf("OPTIONS:\n");
				printf("    -h, --help   Show this message and exit\n");
				return 0;
			case '?':
				fprintf(stderr, "invalid option %c\n", optopt);
				fprintf(stderr, "see '%s --help' for usage\n", argv[0]);
				return 1;
		}
	}

	const int src_count = argc - optind - 1;
	if (src_count < 1)
	{
		fprintf(stderr, "missing destination operand\n");
		return 1;
	}

	const char* dest = argv[argc - 1];

	if (src_count >= 2)
	{
		struct stat st;
		if (stat(dest, &st) == -1 || !S_ISDIR(st.st_mode))
		{
			fprintf(stderr, "destination is not a directory\n");
			return 1;
		}
	}

	int result = 0;
	for (int i = optind; i < argc - 1; i++)
	{
		if (int ret = do_move(argv[i], dest); ret != 0)
		{
			fprintf(stderr, "%s: %s\n", argv[0], strerror(ret));
			result = 1;
		}
	}

	return result;
}
