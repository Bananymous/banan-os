#include <BAN/Assert.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

mode_t __umask = 0;

int chmod(const char* path, mode_t mode)
{
	return syscall(SYS_CHMOD, path, mode);
}

int fchmod(int fildes, mode_t mode)
{
	return syscall(SYS_FCHMOD, fildes, mode);
}

int fstat(int fildes, struct stat* buf)
{
	return syscall(SYS_FSTAT, fildes, buf);
}

int fstatat(int fd, const char* __restrict path, struct stat* __restrict buf, int flag)
{
	return syscall(SYS_FSTATAT, fd, path, buf, flag);
}

int lstat(const char* __restrict path, struct stat* __restrict buf)
{
	return syscall(SYS_STAT, path, buf, AT_SYMLINK_NOFOLLOW);
}

int stat(const char* __restrict path, struct stat* __restrict buf)
{
	return syscall(SYS_STAT, path, buf, 0);
}

mode_t umask(mode_t cmask)
{
	mode_t old = __umask;
	__umask = cmask;
	return old;
}

int mkdir(const char* path, mode_t mode)
{
	return syscall(SYS_CREATE_DIR, path, __UMASKED_MODE(mode));
}
