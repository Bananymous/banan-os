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

	return 0;
}

int test1_job1()
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

	printf("got:       %zu\n", sum);

	exit(0);
}

int test1_job2()
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

int test1()
{
	if (int ret = prepare_file())
		return ret;

	pid_t pid = fork();
	if (pid == 0)
		return test1_job1();

	if (pid == -1)
	{
		perror("fork");
		return 1;
	}

	int ret = test1_job2();
	waitpid(pid, nullptr, 0);
	return ret;
}

int test2_job1()
{
	sleep(2);

	int fd = open(FILE_NAME, O_RDWR);
	if (fd == -1)
	{
		perror("open");
		return 1;
	}

	size_t value = 0;
	if (read(fd, &value, sizeof(size_t)) == -1)
	{
		perror("read");
		return 1;
	}

	printf("got:       %zu\n", value);

	close(fd);

	exit(0);
}

int test2_job2()
{
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

	*(size_t*)addr = 0x12345678;

	if (msync(addr, sizeof(size_t), MS_SYNC) == -1)
	{
		perror("msync");
		return 1;
	}

	printf("expecting: %zu\n", *(size_t*)addr);

	sleep(4);

	munmap(addr, FILE_SIZE);
	close(fd);

	return 0;
}

int test2()
{
	if (int ret = prepare_file())
		return ret;

	pid_t pid = fork();
	if (pid == 0)
		return test2_job1();

	if (pid == -1)
	{
		perror("fork");
		return 1;
	}

	int ret = test2_job2();
	waitpid(pid, nullptr, 0);
	return ret;
}

int main()
{
	test1();
	test2();
}
