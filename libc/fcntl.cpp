#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <unistd.h>

int open(const char* path, int oflag, ...)
{
	va_list args;
	va_start(args, oflag);
	mode_t mode = va_arg(args, mode_t);
	va_end(args);

	return syscall(SYS_OPEN, path, oflag, mode);
}

int openat(int fd, const char* path, int oflag, ...)
{
	va_list args;
	va_start(args, oflag);
	mode_t mode = va_arg(args, mode_t);
	va_end(args);

	return syscall(SYS_OPENAT, fd, path, oflag, mode);
}

int fcntl(int fildes, int cmd, ...)
{
	va_list args;
	va_start(args, cmd);
	int extra = va_arg(args, int);
	va_end(args);

	return syscall(SYS_FCNTL, fildes, cmd, extra);
}