#include <BAN/Assert.h>

#include <ftw.h>

int ftw(const char*, int (*)(const char*, const struct stat*, int), int)
{
	ASSERT_NOT_REACHED();
}

int nftw(const char*, int (*)(const char*, const struct stat*, int, struct FTW*), int, int)
{
	ASSERT_NOT_REACHED();
}
