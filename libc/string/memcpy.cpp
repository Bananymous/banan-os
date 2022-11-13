#include <string.h>

void* memcpy(void* __restrict dstp, const void* __restrict srcp, size_t n)
{
	unsigned char* dst = static_cast<unsigned char*>(dstp);
	const unsigned char* src = static_cast<const unsigned char*>(srcp);
	for (size_t i = 0; i < n; i++)
		dst[i] = src[i];
	return dstp;
}