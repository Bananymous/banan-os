#include <errno.h>

static int s_errno = 0;

int* __errno_location()
{
	return &s_errno;
}
