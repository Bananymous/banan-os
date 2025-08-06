#include <ifaddrs.h>

#include <BAN/Assert.h>

int getifaddrs(struct ifaddrs** ifap)
{
	(void)ifap;
	ASSERT_NOT_REACHED();
}

void freeifaddrs(struct ifaddrs*  ifa)
{
	(void)ifa;
	ASSERT_NOT_REACHED();
}
