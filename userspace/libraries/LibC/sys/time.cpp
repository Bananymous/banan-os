#include <fcntl.h>
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

int getitimer(int which, struct itimerval* value)
{
	return setitimer(which, nullptr, value);
}

int setitimer(int which, const struct itimerval* __restrict value, struct itimerval* __restrict ovalue)
{
	return syscall(SYS_SETITIMER, which, value, ovalue);
}

int utimes(const char* path, const struct timeval times[2])
{
	if (times == nullptr)
		return utimensat(AT_FDCWD, path, nullptr, 0);
	const timespec times_ts[2] {
		timespec {
			.tv_sec = times[0].tv_sec,
			.tv_nsec = times[0].tv_usec * 1000,
		},
		timespec {
			.tv_sec = times[1].tv_sec,
			.tv_nsec = times[1].tv_usec * 1000,
		},
	};
	return utimensat(AT_FDCWD, path, times_ts, 0);
}
