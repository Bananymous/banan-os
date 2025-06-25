#include <sys/file.h>
#include <sys/syscall.h>
#include <unistd.h>

int flock(int fd, int op)
{
	return syscall(SYS_FLOCK, fd, op);
}
