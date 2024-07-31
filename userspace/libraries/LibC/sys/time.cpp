#include <sys/time.h>
#include <time.h>

int gettimeofday(struct timeval* __restrict tp, void* __restrict tzp)
{
	// If tzp is not a null pointer, the behavior is unspecified.
	(void)tzp;

	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	tp->tv_sec = ts.tv_sec;
	tp->tv_usec = ts.tv_nsec / 1000;
	return 0;
}
