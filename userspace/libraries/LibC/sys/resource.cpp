#include <BAN/Assert.h>
#include <BAN/Limits.h>

#include <kernel/Thread.h>

#include <errno.h>
#include <limits.h>
#include <sys/resource.h>

int getrlimit(int resource, struct rlimit* rlp)
{
	switch (resource)
	{
		case RLIMIT_CORE:
			rlp->rlim_cur = 0;
			rlp->rlim_max = 0;
			return 0;
		case RLIMIT_CPU:
			rlp->rlim_cur = BAN::numeric_limits<rlim_t>::max();
			rlp->rlim_max = BAN::numeric_limits<rlim_t>::max();
			return 0;
		case RLIMIT_DATA:
			rlp->rlim_cur = BAN::numeric_limits<rlim_t>::max();
			rlp->rlim_max = BAN::numeric_limits<rlim_t>::max();
			return 0;
		case RLIMIT_FSIZE:
			rlp->rlim_cur = BAN::numeric_limits<rlim_t>::max();
			rlp->rlim_max = BAN::numeric_limits<rlim_t>::max();
			return 0;
		case RLIMIT_NOFILE:
			rlp->rlim_cur = OPEN_MAX;
			rlp->rlim_max = OPEN_MAX;
			return 0;
		case RLIMIT_STACK:
			rlp->rlim_cur = Kernel::Thread::userspace_stack_size;
			rlp->rlim_max = Kernel::Thread::userspace_stack_size;
			return 0;
		case RLIMIT_AS:
			rlp->rlim_cur = BAN::numeric_limits<rlim_t>::max();
			rlp->rlim_max = BAN::numeric_limits<rlim_t>::max();
			return 0;
	}

	errno = EINVAL;
	return -1;
}

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

int setrlimit(int resource, const struct rlimit* rlp)
{
	dwarnln("TODO: setrlimit({}, {})", resource, rlp);
	errno = ENOTSUP;
	return -1;
}

int getpriority(int which, id_t who)
{
	dwarnln("TODO: getpriority({}, {}, {})", which, who);
	errno = ENOTSUP;
	return -1;
}

int setpriority(int which, id_t who, int value)
{
	dwarnln("TODO: setpriority({}, {}, {})", which, who, value);
	errno = ENOTSUP;
	return -1;
}
