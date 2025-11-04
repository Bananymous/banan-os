#include <getopt.h>
#include <stdio.h>
#include <sys/utsname.h>

int main(int argc, char** argv)
{
	bool machine = false;
	bool nodename = false;
	bool release = false;
	bool systemname = false;
	bool version = false;

	for (;;)
	{
		static option long_options[] {
			{ "all",            no_argument, nullptr, 'a' },
			{ "machine",        no_argument, nullptr, 'm' },
			{ "kernel-release", no_argument, nullptr, 'r' },
			{ "nodename",       no_argument, nullptr, 'n' },
			{ "kernel-name",    no_argument, nullptr, 's' },
			{ "kernel-version", no_argument, nullptr, 'v' },
			{ "help",           no_argument, nullptr,  0  },
		};

		int ch = getopt_long(argc, argv, "amnrsv", long_options, nullptr);
		if (ch == -1)
			break;

		switch (ch)
		{
			case 0:
				printf("usage: %s [OPTION]...\n", argv[0]);
				printf("  -a, --all        print all information\n");
				printf("  -m, --machine    print machine name\n");
				printf("  -r, --release    print release\n");
				printf("  -n, --nodename   print node name\n");
				printf("  -s, --system     print system name\n");
				printf("  -v, --version    print version\n");
				return 0;
			case 'a':
				machine = true;
				nodename = true;
				release = true;
				systemname = true;
				version = true;
				break;
			case 'm':
				machine = true;
				break;
			case 'n':
				nodename = true;
				break;
			case 'r':
				release = true;
				break;
			case 's':
				systemname = true;
				break;
			case 'v':
				version = true;
				break;
			case '?':
				fprintf(stderr, "invalid option %c\n", optopt);
				fprintf(stderr, "see '%s --help' for usage\n", argv[0]);
				return 1;
		}
	}

	if (!machine && !nodename && !release && !systemname && !version)
		systemname = true;

	utsname utsname;
	if (uname(&utsname) == -1)
	{
		perror("uname: ");
		return 1;
	}

	if (systemname)
		printf("%s ", utsname.sysname);
	if (nodename)
		printf("%s ", utsname.nodename);
	if (release)
		printf("%s ", utsname.release);
	if (version)
		printf("%s ", utsname.version);
	if (machine)
		printf("%s ", utsname.machine);
	printf("\n");
}
