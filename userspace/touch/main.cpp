#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

int main(int argc, char** argv)
{
	int ret = 0;
	for (int i = 1; i < argc; i++)
	{
		if (creat(argv[i], 0644) == -1 && errno != EEXIST)
		{
			perror(argv[i]);
			ret = 1;
		}
	}
	return ret;
}
