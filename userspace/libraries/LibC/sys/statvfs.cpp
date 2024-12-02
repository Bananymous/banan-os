#include <fcntl.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>

static int fstatvfsat(int fildes, const char* path, struct statvfs* buf)
{
	return syscall(SYS_FSTATVFSAT, fildes, path, buf);
}

int fstatvfs(int fildes, struct statvfs* buf)
{
	return fstatvfsat(fildes, nullptr, buf);
}

int statvfs(const char* __restrict path, struct statvfs* __restrict buf)
{
	return fstatvfsat(AT_FDCWD, path, buf);
}
