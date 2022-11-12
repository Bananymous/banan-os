#include <string.h>

void* memcpy(void* restrict destp, const void* restrict srcp, size_t n)
{
	unsigned char* dest = (unsigned char*)destp;
	const unsigned char* src = (const unsigned char*)srcp;
	for (size_t i = 0; i < n; i++)
		dest[i] = src[i];
	return destp;
}