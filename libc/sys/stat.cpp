#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

int fstat(int fildes, struct stat* buf)
{
	return syscall(SYS_FSTAT, fildes, buf);
}

int fstatat(int fd, const char* __restrict path, struct stat* __restrict buf, int flag)
{
	if (flag == AT_SYMLINK_NOFOLLOW)
		flag = O_NOFOLLOW;
	else if (flag)
	{
		errno = EINVAL;
		return -1;
	}

	int target = openat(fd, path, O_SEARCH | flag);
	if (target == -1)
		return -1;
	int ret = fstat(target, buf);
	close(target);
	return ret;
}

int lstat(const char* __restrict path, struct stat* __restrict buf)
{
	int fd = open(path, O_SEARCH | O_NOFOLLOW);
	if (fd == -1)
		return -1;
	int ret = fstat(fd, buf);
	close(fd);
	return ret;
}

int stat(const char* __restrict path, struct stat* __restrict buf)
{
	int fd = open(path, O_SEARCH);
	if (fd == -1)
		return -1;
	int ret = fstat(fd, buf);
	close(fd);
	return ret;
}
