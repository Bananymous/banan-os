#include <errno.h>
#include <sys/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

int futex(int op, const uint32_t* addr, uint32_t value, const struct timespec* abstime)
{
	errno = 0;
	while (syscall(SYS_FUTEX, op, addr, value, abstime) == -1 && errno == EINTR)
		errno = 0;
	return errno;
}
