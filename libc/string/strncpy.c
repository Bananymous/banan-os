#include <string.h>

char* strncpy(char* restrict dest, const char* restrict src, size_t n)
{
	size_t i;
	for (i = 0; src[i] && i < n; i++)
		dest[i] = src[i];
	for (; i < n; i++)
		dest[i] = '\0';
	return dest;
}