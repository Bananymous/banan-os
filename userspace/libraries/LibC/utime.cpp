#include <fcntl.h>
#include <sys/stat.h>
#include <utime.h>

int utime(const char* filename, const struct utimbuf* times)
{
	if (times == nullptr)
		return utimensat(AT_FDCWD, filename, nullptr, 0);
	const timespec times_ts[2] {
		timespec {
			.tv_sec = times->actime,
			.tv_nsec = 0,
		},
		timespec {
			.tv_sec = times->modtime,
			.tv_nsec = 0,
		},
	};
	return utimensat(AT_FDCWD, filename, times_ts, 0);
}
