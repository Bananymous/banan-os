#include <string.h>

void foo()
{
	strlen(nullptr);
	strncpy(nullptr, nullptr, 0);
	memcpy(nullptr, nullptr, 0);
}