#include <getopt.h>
#include <libgen.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
	bool zero = false;

	for (;;)
	{
		static option long_options[] {
			{ "zero", no_argument, nullptr, 'z' },
			{ "help", no_argument, nullptr, 'h' },
		};

		int ch = getopt_long(argc, argv, "zh", long_options, nullptr);
		if (ch == -1)
			break;

		switch (ch)
		{
			case 'z':
				zero = true;
				break;
			case 'h':
				fprintf(stderr, "usage: %s [OPTIONS]...\n", argv[0]);
				fprintf(stderr, "  control the audio server\n");
				fprintf(stderr, "OPTIONS:\n");
				fprintf(stderr, "  -l, --list      list devices and their pins\n");
				fprintf(stderr, "  -d, --device N  set device index N as the current one\n");
				fprintf(stderr, "  -p, --pin N     set pin N as the current one\n");
				fprintf(stderr, "  -v, --volume N  set volume to N%%. if + or - is given, volume is relative to the current volume\n");
				fprintf(stderr, "  -h, --help      show this message and exit\n");
				return 0;
			case '?':
				fprintf(stderr, "invalid option %c\n", optopt);
				fprintf(stderr, "see '%s --help' for usage\n", argv[0]);
				return 1;
		}
	}

	if (optind >= argc)
	{
		fprintf(stderr, "missing operand\n");
		fprintf(stderr, "see '%s --help' for usage\n", argv[0]);
		return 1;
	}

	for (int i = optind; i < argc; i++)
	{
		printf("%s", dirname(argv[i]));
		putchar(zero ? '\0' : '\n');
	}

	return 0;
}
