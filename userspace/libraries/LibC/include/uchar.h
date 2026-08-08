#ifndef _UCHAR_H
#define _UCHAR_H 1

// https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/uchar.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

size_t c16rtomb(char* __restrict__ s, char16_t c16, mbstate_t* __restrict__ ps);
size_t c32rtomb(char* __restrict__ s, char32_t c32, mbstate_t* __restrict__ ps);
size_t mbrtoc16(char16_t* __restrict__ pc16, const char* __restrict__ s, size_t n, mbstate_t* __restrict__ ps);
size_t mbrtoc32(char32_t* __restrict__ pc32, const char* __restrict__ s, size_t n, mbstate_t* __restrict__ ps);

__END_DECLS

#endif
