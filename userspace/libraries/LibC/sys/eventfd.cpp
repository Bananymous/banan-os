#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <unistd.h>

int eventfd(unsigned int initval, int flags)
{
	return syscall(SYS_EVENTFD, initval, flags);
}
