#include <string.h>

int strncmp(const char* s1, const char* s2, size_t n)
{
	const unsigned char* u1 = (unsigned char*)s1;
	const unsigned char* u2 = (unsigned char*)s2;
	for (; --n && *u1 && *u2; u1++, u2++)
		if (*u1 != *u2)
			break;
	return *u1 - *u2;
}
