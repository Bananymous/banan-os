#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

int main(int argc, char** argv)
{
	for (int i = 1; i < argc; i++)
	{
		int fd = open(argv[i], O_WRONLY | O_CREAT, 0644);
		if (fd == -1)
		{
			if (errno != EEXISTS)
				perror(argv[i]);
		}
		else
		{
			close(fd);
		}
	}
	return 0;
}
