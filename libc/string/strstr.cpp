#include <string.h>

char* strstr(const char* haystack, const char* needle)
{
	for (size_t i = 0; haystack[i]; i++)
		if (memcmp(haystack + i, needle, strlen(needle)) == 0)
			return (char*)haystack + i;
	return NULL;
}