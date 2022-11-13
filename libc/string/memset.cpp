#include <string.h>

void* memset(void* s, int c, size_t n)
{
	unsigned char* p = static_cast<unsigned char*>(s);
	for (size_t i = 0; i < n; i++)
		p[i] = c;
	return s;
}