#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

int clock_gettime(clockid_t clock_id, struct timespec* tp)
{
	return syscall(SYS_CLOCK_GETTIME, clock_id, tp);
}

int nanosleep(const struct timespec* rqtp, struct timespec* rmtp)
{
	return syscall(SYS_NANOSLEEP, rqtp, rmtp);
}

time_t time(time_t* tloc)
{
	timespec tp;
	if (clock_gettime(CLOCK_REALTIME, &tp) == -1)
		return -1;
	if (tloc)
		*tloc = tp.tv_sec;
	return tp.tv_sec;
}
