#include <errno.h>
#include <pthread.h>

int* __errno_location()
{
	return &_get_uthread()->errno_;
}
