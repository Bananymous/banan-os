#include <errno.h>
#include <pthread.h>

extern uthread* _get_uthread();

int* __errno_location()
{
	return &_get_uthread()->errno_;
}
