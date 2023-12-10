#include <fcntl.h>
#include <stdio.h>

bool cat_file(int fd)
{
	char buffer[1024];
	while (ssize_t n_read = read(fd, buffer, sizeof(buffer)))
	{
		if (n_read == -1)
		{
			perror("read");
			return false;
		}
		write(STDOUT_FILENO, buffer, n_read);
	}
	return true;
}

int main(int argc, char** argv)
{
	int ret = 0;

	if (argc > 1)
	{
		for (int i = 1; i < argc; i++)
		{
			int fd = open(argv[i], O_RDONLY);
			if (fd == -1)
			{
				perror(argv[i]);
				ret = 1;
				continue;
			}
			if (!cat_file(fd))
				ret = 1;
			close(fd);
		}
	}
	else
	{
		if (!cat_file(STDIN_FILENO))
			ret = 1;
	}

	return ret;
}
