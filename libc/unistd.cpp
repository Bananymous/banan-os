#include <unistd.h>

pid_t fork(void)
{
	return -1;
}

int execv(const char*, char* const[])
{
	return -1;
}

int execve(const char*, char* const[], char* const[])
{
	return -1;
}

int execvp(const char*, char* const[])
{
	return -1;
}

pid_t getpid(void)
{
	return -1;
}
