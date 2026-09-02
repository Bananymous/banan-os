#include <fcntl.h>
#include <stdio.h>
#include <string.h>

static bool cat_file(int fd)
{
	char last = '\n';
	char buffer[1024];
	while (ssize_t n_read = read(fd, buffer, sizeof(buffer)))
	{
		if (n_read == -1)
			return false;
		write(STDOUT_FILENO, buffer, n_read);
		last = buffer[n_read - 1];
	}
	if (last != '\n')
		write(STDOUT_FILENO, "\n", 1);
	return true;
}

int main(int argc, const char** argv)
{
	if (argc < 2)
	{
		argv[1] = "-";
		argc = 2;
	}

	int ret = 0;
	for (int i = 1; i < argc; i++)
	{
		int fd = (strcmp(argv[i], "-") == 0)
			? STDIN_FILENO
			: open(argv[i], O_RDONLY);

		if (fd == -1 || !cat_file(fd))
		{
			printf("%s: %s: %m\n", argv[0], argv[i]);
			ret = 1;
		}

		if (fd != STDIN_FILENO)
			close(fd);
	}

	return ret;
}
