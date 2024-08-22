#include <BAN/Assert.h>
#include <syslog.h>

void openlog(const char*, int, int)
{
	ASSERT_NOT_REACHED();
}

void syslog(int, const char*, ...)
{
	ASSERT_NOT_REACHED();
}
