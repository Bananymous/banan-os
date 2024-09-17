#include <BAN/Assert.h>
#include <errno.h>
#include <sys/resource.h>

int getrusage(int who, struct rusage* r_usage)
{
	if (who != RUSAGE_CHILDREN && who != RUSAGE_SELF)
	{
		errno = EINVAL;
		return -1;
	}

	r_usage->ru_stime.tv_sec = 0;
	r_usage->ru_stime.tv_usec = 0;

	r_usage->ru_utime.tv_sec = 0;
	r_usage->ru_utime.tv_usec = 0;

	return 0;
}
