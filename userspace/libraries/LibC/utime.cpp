#include <BAN/Assert.h>

#include <utime.h>

int utime(const char*, const struct utimbuf*)
{
	ASSERT_NOT_REACHED();
}
