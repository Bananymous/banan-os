#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#define MAX_FILES 20
#define BUF_SIZE 1024

int main(int argc, char** argv)
{
	int files[MAX_FILES] {};
	size_t file_count = 0;

	int arg = 1;

	int oflag = O_WRONLY | O_CREAT;
	if (arg < argc && strcmp(argv[arg], "-a") == 0)
	{
		oflag |= O_APPEND;
		arg++;
	}
	else
	{
		oflag |= O_TRUNC;
	}

	for (int i = arg; i < argc; i++)
	{
		files[file_count] = open(argv[i], oflag, 0644);
		if (files[file_count] == -1)
			perror(argv[i]);
		else
			file_count++;

		if (file_count >= MAX_FILES)
		{
			fprintf(stderr, "only up to %d files are supported\n", MAX_FILES);
			break;
		}
	}

	char* buffer = (char*)malloc(BUF_SIZE);
	for (;;)
	{
		ssize_t nread = read(STDIN_FILENO, buffer, BUF_SIZE);
		if (nread == -1)
			perror("stdin");
		if (nread <= 0)
			break;
		write(STDOUT_FILENO, buffer, nread);
		for (size_t i = 0; i < file_count; i++)
			write(files[i], buffer, nread);
	}
	free(buffer);

	if (ferror(stdin))
		perror("stdin");

	for (size_t i = 0; i < file_count; i++)
		close(files[i]);

	return 0;
}
