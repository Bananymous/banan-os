#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void usage(int ret, char* cmd)
{
	FILE* fout = (ret == 0) ? stdout : stderr;
	fprintf(fout, "usage: %s [OPTION]...\n", cmd);
	fprintf(fout, "Tells the kernel to start a disk sync as soon as possible\n");
	fprintf(fout, "    -b, --block     return only after sync is complete\n");
	fprintf(fout, "    -h, --help      show this message and exit\n");
	exit(ret);
}

int main(int argc, char** argv)
{
	bool block = false;
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--block") == 0)
			block = true;
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
			usage(0, argv[0]);
		else
			usage(1, argv[0]);
	}
	syncsync(block);
	return 0;
}
