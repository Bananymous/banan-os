#include <fcntl.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>

int open(const char* path, int oflag, ...)
{
	return syscall(SYS_OPEN, path, oflag);
}

int openat(int fd, const char* path, int oflag, ...)
{
	return syscall(SYS_OPENAT, fd, path, oflag);
}
