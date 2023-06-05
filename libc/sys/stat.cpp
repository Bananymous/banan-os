#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

int lstat(const char* __restrict path, struct stat* __restrict buf)
{
    return syscall(SYS_STAT, path, buf, O_RDONLY | O_NOFOLLOW);
}

int stat(const char* __restrict path, struct stat* __restrict buf)
{
    return syscall(SYS_STAT, path, buf, O_RDONLY);
}
