#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static const char* s_argv0 { nullptr };

static bool truncate_file(const char* path, uint64_t size, bool io_blocks, bool no_create)
{
	const int fd = open(path, O_WRONLY | (no_create ? 0 : O_CREAT), 0644);
	if (fd == -1)
	{
		if (errno == ENOENT && no_create)
			return true;
		fprintf(stderr, "%s: cannot open '%s': %m\n", s_argv0, path);
		return false;
	}

	if (io_blocks)
	{
		struct stat st;
		if (fstat(fd, &st) == -1)
		{
			fprintf(stderr, "%s: cannot stat '%s': %m\n", s_argv0, path);
			close(fd);
			return false;
		}
		size *= st.st_blksize;
	}

	if (ftruncate(fd, size) == -1)
	{
		fprintf(stderr, "%s: cannot truncate '%s': %m\n", s_argv0, path);
		close(fd);
		return false;
	}

	close(fd);

	return true;
}

int main(int argc, char* argv[])
{
	s_argv0 = argv[0];

	bool no_create { false };
	bool io_blocks { false };
	uint64_t size { UINT64_MAX };

	for (;;)
	{
		static option long_options[] {
			{ "no-create", no_argument,       nullptr, 'c' },
			{ "io-blocks", no_argument,       nullptr, 'o' },
			{ "size",      required_argument, nullptr, 's' },
			{ "help",      no_argument,       nullptr,  0  },
			{}
		};

		int ch = getopt_long(argc, argv, "cos:", long_options, nullptr);
		if (ch == -1)
			break;

		switch (ch)
		{
			case 'c':
				no_create = true;
				break;
			case 'o':
				io_blocks = true;
				break;
			case 's':
			{
				char* endptr = nullptr;
				size = strtoull(optarg, &endptr, 0);
				if (*endptr != '\0')
				{
					bool valid_suffix { false };
					const char suffixes[] { 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y', 'R', 'Q' };
					for (size_t i = 0; i < sizeof(suffixes) / sizeof(*suffixes); i++)
					{
						if (*endptr != suffixes[i])
							continue;

						uint64_t base;
						if (strcmp(endptr + 1, "") == 0 || strcmp(endptr + 1, "iB") == 0)
							base = 1024;
						else if (strcmp(endptr + 1, "B") == 0)
							base = 1000;
						else
							break;

						uint64_t mult = base;
						for (size_t j = 0; j < i; j++)
							mult *= base;

						size *= mult;
						valid_suffix = true;

						break;
					}

					if (!valid_suffix)
					{
						fprintf(stderr, "%s: invalid size '%s'\n", argv[0], optarg);
						fprintf(stderr, "see '%s --help' for usage\n", argv[0]);
						return 1;
					}
				}
				break;
			}
			case 0:
				fprintf(stderr, "usage: %s [OPTION]... FILE...\n", argv[0]);
				fprintf(stderr, "  shrink or extend each FILE to given size\n");
				fprintf(stderr, "OPTIONS:\n");
				fprintf(stderr, "  -c, --no-create    do not create files if they do not exists\n");
				fprintf(stderr, "  -o, --io-blocks    thread SIZE as number of io blocks instead of bytes\n");
				fprintf(stderr, "  -s, --size=SIZE    set FILE size to this value\n");
				fprintf(stderr, "      --help         show this message and exit\n");
				return 0;
			case ':' : case '?':
				fprintf(stderr, "see '%s --help' for usage\n", argv[0]);
				return 1;
		}
	}

	if (size == UINT64_MAX)
	{
		fprintf(stderr, "%s: you must specify --size\n", argv[0]);
		fprintf(stderr, "see '%s --help' for usage\n", argv[0]);
		return 1;
	}

	if (optind >= argc)
	{
		fprintf(stderr, "%s: missing operand\n", argv[0]);
		fprintf(stderr, "see '%s --help' for usage\n", argv[0]);
		return 1;
	}

	int ret = 0;
	for (int i = optind; i < argc; i++)
		if (!truncate_file(argv[i], size, io_blocks, no_create))
			ret = 1;

	return ret;
}
