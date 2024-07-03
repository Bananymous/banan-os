#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CURRENT_NS() ({ timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); ts.tv_sec * 1'000'000'000 + ts.tv_nsec; })

uint64_t parse_sized_u64(const char* val)
{
	uint64_t result = 0;

	const char* ptr = val;
	for (; *ptr && isdigit(*ptr); ptr++)
		result = (result * 10) + (*ptr - '0');

	switch (*ptr)
	{
		case 'E': result *= 1024; // fall through
		case 'P': result *= 1024; // fall through
		case 'T': result *= 1024; // fall through
		case 'G': result *= 1024; // fall through
		case 'M': result *= 1024; // fall through
		case 'K': case 'k': result *= 1024; ptr++; break;
	}

	if (*ptr != '\0')
	{
		fprintf(stderr, "invalid number: %s\n", val);
		exit(1);
	}

	return result;
}

void print_value_with_unit(uint64_t value_x10, unsigned base, const char* units[])
{
	unsigned index = 0;
	while (value_x10 / 10 >= base)
	{
		index++;
		value_x10 /= base;
	}
	if (value_x10 < 100)
		printf("%u.%u %s", (unsigned)value_x10 / 10, (unsigned)value_x10 % 10, units[index]);
	else
		printf("%u %s", (unsigned)value_x10 / 10, units[index]);

}

void print_time(uint64_t start_ns, uint64_t end_ns, uint64_t transfered, bool last = false)
{
	static bool first = true;
	if (!first)
		printf("\e[F");
	first = false;

	printf("%" PRIu64 " bytes", transfered);
	if (transfered >= 1000)
	{
		printf(" (");
		{
			const char* units[] { "", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB", "RB", "QB" };
			print_value_with_unit(transfered * 10, 1000, units);
		}
		if (transfered >= 1024)
		{
			printf(", ");
			const char* units[] { "", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB", "RiB", "QiB" };
			print_value_with_unit(transfered * 10, 1024, units);
		}
		printf(")");
	}
	printf(" copied");

	double duration_s = (end_ns - start_ns) / 1e9;
	if (last)
		printf(", %f s, ", duration_s);
	else
		printf(", %u s, ", (unsigned)duration_s);

	const char* units[] { "B/s", "kB/s", "MB/s", "GB/s", "TB/s", "PB/s", "EB/s", "ZB/s", "YB/s", "RB/s", "QB/s" };
	print_value_with_unit(10 * transfered / duration_s, 1000, units);

	printf("\e[K\n");
}

int main(int argc, char** argv)
{
	const char* input = nullptr;
	const char* output = nullptr;
	uint64_t bs = 512;
	uint64_t count = ~(uint64_t)0;
	bool print_progress = false;

	for (int i = 1; i < argc; i++)
	{
		if (strncmp(argv[i], "if=", 3) == 0)
			input = argv[i] + 3;
		else if (strncmp(argv[i], "of=", 3) == 0)
			output = argv[i] + 3;
		else if (strncmp(argv[i], "bs=", 3) == 0)
			bs = parse_sized_u64(argv[i] + 3);
		else if (strncmp(argv[i], "count=", 6) == 0)
			count = parse_sized_u64(argv[i] + 6);
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

	uint64_t total_transfered = 0;
	uint64_t start_ns = CURRENT_NS();
	uint64_t last_print_s = 0;

	for (uint64_t i = 0; i != count; i++)
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

		if ((size_t)nread < bs || (size_t)nwrite < bs)
			break;

		if (print_progress)
		{
			uint64_t current_ns = CURRENT_NS();
			if ((current_ns - start_ns) / 1'000'000'000 > last_print_s)
			{
				print_time(start_ns, current_ns, total_transfered);
				last_print_s = (current_ns - start_ns) / 1'000'000'000;
			}
		}
	}

	print_time(start_ns, CURRENT_NS(), total_transfered, true);

	close(ifd);
	close(ofd);
	free(buffer);

	return 0;
}
