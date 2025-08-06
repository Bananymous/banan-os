#include <sys/times.h>

#include <BAN/Assert.h>

clock_t times(struct tms* buffer)
{
	(void)buffer;
	ASSERT_NOT_REACHED();
}
