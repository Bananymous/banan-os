#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CURRENT_NS() ({ timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); ts.tv_sec * 1'000'000'000 + ts.tv_nsec; })

int parse_int(const char* val)
{
	int result = 0;

	const char* ptr = val;
	while (*ptr)
	{
		if (!isdigit(*ptr))
		{
			fprintf(stderr, "invalid number: %s\n", val);
			exit(1);
		}
		result = (result * 10) + (*ptr - '0');
		*ptr++;
	}

	return result;
}

void print_time(uint64_t start_ns, uint64_t end_ns, int transfered)
{
	static bool first = true;
	uint64_t duration_ns = end_ns - start_ns;
	printf("%s%d bytes copied, %d.%09d s\e[K\n", (first ? "" : "\e[F"), transfered, (int)(duration_ns / 1'000'000'000), (int)(duration_ns % 1'000'000'000));
	first = false;
}

int main(int argc, char** argv)
{
	const char* input = nullptr;
	const char* output = nullptr;
	int bs = 512;
	int count = -1;
	bool print_progress = false;

	for (int i = 1; i < argc; i++)
	{
		if (strncmp(argv[i], "if=", 3) == 0)
			input = argv[i] + 3;
		else if (strncmp(argv[i], "of=", 3) == 0)
			output = argv[i] + 3;
		else if (strncmp(argv[i], "bs=", 3) == 0)
			bs = parse_int(argv[i] + 3);
		else if (strncmp(argv[i], "count=", 6) == 0)
			count = parse_int(argv[i] + 6);
		else if (strcmp(argv[i], "status=progress") == 0)
			print_progress = true;
		else
		{
			fprintf(stderr, "unrecognized option: %s\n", argv[i]);
			exit(1);
		}
	}

	int ifd = STDIN_FILENO;
	if (input)
	{
		ifd = open(input, O_RDONLY);
		if (ifd == -1)
		{
			perror("open");
			return 1;
		}
	}

	int ofd = STDIN_FILENO;
	if (output)
	{
		ofd = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (ofd == -1)
		{
			perror("open");
			return 1;
		}
	}

	uint8_t* buffer = (uint8_t*)malloc(bs);
	if (buffer == nullptr)
	{
		perror("malloc");
		return 1;
	}

	size_t total_transfered = 0;

	uint64_t start_ns = CURRENT_NS();
	uint64_t last_print_ns = 0;

	for (int i = 0; i != count; i++)
	{
		ssize_t nread = read(ifd, buffer, bs);
		if (nread == -1)
		{
			perror("read");
			return 1;
		}

		ssize_t nwrite = write(ofd, buffer, nread);
		if (nwrite == -1)
		{
			perror("write");
			return 1;
		}

		total_transfered += nwrite;

		if (nread < bs || nwrite < bs)
			break;

		if (print_progress)
		{
			uint64_t current_ns = CURRENT_NS();
			if (current_ns >= last_print_ns + 1'000'000'000)
			{
				print_time(start_ns, current_ns, total_transfered);
				last_print_ns = current_ns;
			}
		}
	}

	print_time(start_ns, CURRENT_NS(), total_transfered);

	close(ifd);
	close(ofd);
	free(buffer);

	return 0;
}
