#include <errno.h>

#if __disable_thread_local_storage
static int s_errno = 0;
#else
static thread_local int s_errno = 0;
#endif

int* __errno_location()
{
	return &s_errno;
}
