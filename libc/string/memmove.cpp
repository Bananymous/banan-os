#include <string.h>

void* memmove(void* destp, const void* srcp, size_t n)
{
	unsigned char* dest = static_cast<unsigned char*>(destp);
	const unsigned char* src = static_cast<const unsigned char*>(srcp);

	if (dest < src)
	{
		for (size_t i = 0; i < n; i++)
			dest[i] = src[i];
	}
	else
	{
		for (size_t i = 1; i <= n; i++)
			dest[n - i] = src[n - i];
	}

	return destp;
}
