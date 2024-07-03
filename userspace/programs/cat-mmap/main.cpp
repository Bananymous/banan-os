#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>

bool cat_file(int fd)
{
	struct stat st;
	if (fstat(fd, &st) == -1)
	{
		perror("stat");
		return false;
	}

	void* addr = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (addr == MAP_FAILED)
	{
		perror("mmap");
		return false;
	}

	ssize_t nwrite = write(STDOUT_FILENO, addr, st.st_size);
	if (nwrite == -1)
		perror("write");

	if (static_cast<uint8_t*>(addr)[st.st_size - 1] != '\n')
		if (write(STDOUT_FILENO, "\n", 1) == -1)
			perror("write");

	if (munmap(addr, st.st_size) == -1)
	{
		perror("munmap");
		return false;
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
