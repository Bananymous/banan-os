#include <ctype.h>
#include <string.h>
#include <strings.h>

int ffs(int i)
{
	for (unsigned idx = 0; idx < sizeof(i) * 8; idx++)
		if (i & (1 << idx))
			return i + 1;
	return 0;
}

int strcasecmp(const char* s1, const char* s2)
{
	const unsigned char* u1 = (unsigned char*)s1;
	const unsigned char* u2 = (unsigned char*)s2;
	for (; *u1 && *u2; u1++, u2++)
		if (tolower(*u1) != tolower(*u2))
			break;
	return tolower(*u1) - tolower(*u2);
}

int strncasecmp(const char* s1, const char* s2, size_t n)
{
	const unsigned char* u1 = (unsigned char*)s1;
	const unsigned char* u2 = (unsigned char*)s2;
	for (; --n && *u1 && *u2; u1++, u2++)
		if (tolower(*u1) != tolower(*u2))
			break;
	return tolower(*u1) - tolower(*u2);
}

int bcmp(const void* s1, const void* s2, size_t n)
{
	return memcmp(s1, s2, n);
}

void bcopy(const void* src, void* dest, size_t n)
{
	memmove(dest, src, n);
}

void bzero(void* s, size_t n)
{
	memset(s, 0, n);
}
