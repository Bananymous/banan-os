#include <fcntl.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>

int open(const char* path, int oflag, ...)
{
	int ret = syscall(SYS_OPEN, path, oflag);
	if (ret < 0)
	{
		errno = -ret;
		return -1;
	}
	return ret;
}
