#include <inttypes.h>
#include <stdlib.h>

intmax_t strtoimax(const char* __restrict nptr, char** __restrict endptr, int base)
{
	static_assert(sizeof(intmax_t) == sizeof(long long));
	return strtoll(nptr, endptr, base);
}

uintmax_t strtoumax(const char* __restrict nptr, char** __restrict endptr, int base)
{
	static_assert(sizeof(uintmax_t) == sizeof(unsigned long long));
	return strtoull(nptr, endptr, base);
}
