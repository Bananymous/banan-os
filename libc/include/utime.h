#ifndef _UTIME_H
#define _UTIME_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/utime.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_time_t
#include <sys/types.h>

struct utimbuf
{
	time_t actime;	/* Access time. */
	time_t modtime;	/* Modification time. */
};

int utime(const char* path, const struct utimbuf* times);

__END_DECLS

#endif
