#include <sys/syscall.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

int gettimeofday(struct timeval* __restrict tp, void* __restrict tzp)
{
	// If tzp is not a null pointer, the behavior is unspecified.
	if (tzp != nullptr)
		*static_cast<struct timezone*>(tzp) = {};

	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	tp->tv_sec = ts.tv_sec;
	tp->tv_usec = ts.tv_nsec / 1000;
	return 0;
}

int setitimer(int which, const struct itimerval* __restrict value, struct itimerval* __restrict ovalue)
{
	return syscall(SYS_SETITIMER, which, value, ovalue);
}
