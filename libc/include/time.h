#pragma once

#include <sys/types.h>

__BEGIN_DECLS

struct timespec
{
	time_t tv_sec;
	long tv_nsec;
};

__END_DECLS