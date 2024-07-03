#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/banan-os.h>

void usage(int ret, char* arg0)
{
	FILE* fout = (ret == 0) ? stdout : stderr;
	fprintf(fout, "usage: %s [OPTIONS]...\n", arg0);
	fprintf(fout, "  -s, --shutdown  Shutdown the system (default)\n");
	fprintf(fout, "  -r, --reboot    Reboot the system\n");
	fprintf(fout, "  -h, --help      Show this message\n");
	exit(ret);
}

int main(int argc, char** argv)
{
	int operation = POWEROFF_SHUTDOWN;
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--shutdown") == 0)
			operation = POWEROFF_SHUTDOWN;
		else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--reboot") == 0)
			operation = POWEROFF_REBOOT;
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
			usage(0, argv[0]);
		else
			usage(1, argv[0]);
	}

	if (poweroff(operation) == -1)
	{
		perror("poweroff");
		return 1;
	}

	return 0;
}
