#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

int raise(int sig)
{
	return syscall(SYS_RAISE, sig);
}