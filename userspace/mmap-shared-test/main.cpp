#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define FILE_NAME "test-file"
#define FILE_SIZE (1024*1024)

int prepare_file()
{
	int fd = open(FILE_NAME, O_RDWR | O_TRUNC | O_CREAT, 0666);
	if (fd == -1)
	{
		perror("open");
		return 1;
	}

	void* null_buffer = malloc(FILE_SIZE);
	memset(null_buffer, 0x00, FILE_SIZE);

	if (write(fd, null_buffer, FILE_SIZE) == -1)
	{
		perror("write");
		return 1;
	}

	free(null_buffer);
	close(fd);

	printf("file created\n");

	return 0;
}

int job1()
{
	int fd = open(FILE_NAME, O_RDONLY);
	if (fd == -1)
	{
		perror("open");
		return 1;
	}

	void* addr = mmap(nullptr, FILE_SIZE, PROT_READ, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED)
	{
		perror("mmap");
		return 1;
	}

	sleep(4);

	size_t sum = 0;
	for (int i = 0; i < FILE_SIZE; i++)
		sum += ((uint8_t*)addr)[i];

	munmap(addr, FILE_SIZE);
	close(fd);

	printf("sum: %zu\n", sum);

	return 0;
}

int job2()
{
	sleep(2);

	int fd = open(FILE_NAME, O_RDWR);
	if (fd == -1)
	{
		perror("open");
		return 1;
	}

	void* addr = mmap(nullptr, FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED)
	{
		perror("mmap");
		return 1;
	}

	memset(addr, 'a', FILE_SIZE);

	munmap(addr, FILE_SIZE);
	close(fd);

	printf("expecting: %zu\n", (size_t)'a' * FILE_SIZE);

	return 0;
}

int main()
{
	if (int ret = prepare_file())
		return ret;

	pid_t pid = fork();
	if (pid == 0)
		return job1();

	if (pid == -1)
	{
		perror("fork");
		return 1;
	}

	int ret = job2();
	waitpid(pid, nullptr, 0);

	return ret;
}
