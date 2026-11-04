#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int usage(char* argv0, int ret)
{
	FILE* fp = (ret == 0) ? stdout : stderr;
	fprintf(fp, "usage: %s DURATION[SUFFIX]\n", argv0);
	fprintf(fp, "  DURATION can be an integer or floating point\n");
	fprintf(fp, "  SUFFIX can be s, m, h, or d\n");
	return ret;
}

int main(int argc, char** argv)
{
	if (argc != 2)
		return usage(argv[0], 1);

	char* endp;
	auto seconds = ({ errno = 0; strtod(argv[1], &endp); });
	if (errno || seconds < 0)
	{
		fprintf(stderr, "%s: invalid duration `%s'\n", argv[0], argv[1]);
		return 1;
	}

	switch (*endp)
	{
		case '\0':
			break;
		case 'd':
			seconds *= 24.0;
			[[fallthrough]];
		case 'h':
			seconds *= 60.0;
			[[fallthrough]];
		case 'm':
			seconds *= 60.0;
			[[fallthrough]];
		case 's':
			if (endp[1] == '\0')
				break;
			[[fallthrough]];
		default:
			fprintf(stderr, "%s: invalid duration `%s'\n", argv[0], argv[1]);
			return 1;
	}

	const uint64_t nanoseconds = [seconds]() -> uint64_t {
		const auto nanoseconds = seconds * 1e9;
		if (nanoseconds >= UINT64_MAX)
			return UINT64_MAX;
		return nanoseconds;
	}();

	const timespec ts {
		.tv_sec = static_cast<time_t>(nanoseconds / 1'000'000'000),
		.tv_nsec = static_cast<long>(nanoseconds % 1'000'000'000),
	};
	nanosleep(&ts, nullptr);
}
