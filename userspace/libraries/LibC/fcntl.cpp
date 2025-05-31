#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <unistd.h>

int creat(const char* path, mode_t mode)
{
	pthread_testcancel();
	return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

int open(const char* path, int oflag, ...)
{
	pthread_testcancel();

	va_list args;
	va_start(args, oflag);
	mode_t mode = va_arg(args, mode_t);
	va_end(args);

	return openat(AT_FDCWD, path, oflag, mode);
}

int openat(int fd, const char* path, int oflag, ...)
{
	pthread_testcancel();

	va_list args;
	va_start(args, oflag);
	mode_t mode = va_arg(args, mode_t);
	va_end(args);

	return syscall(SYS_OPENAT, fd, path, oflag, __UMASKED_MODE(mode));
}

int fcntl(int fildes, int cmd, ...)
{
	pthread_testcancel();

	va_list args;
	va_start(args, cmd);
	int extra = va_arg(args, int);
	va_end(args);

	return syscall(SYS_FCNTL, fildes, cmd, extra);
}
