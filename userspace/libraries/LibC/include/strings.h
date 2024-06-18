#ifndef _STRINGS_H
#define _STRINGS_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/strings.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <bits/types/locale_t.h>

#define __need_size_t
#include <sys/types.h>

int ffs(int i);
int strcasecmp(const char* s1, const char* s2);
int strcasecmp_l(const char* s1, const char* s2, locale_t locale);
int strncasecmp(const char* s1, const char* s2, size_t n);
int strncasecmp_l(const char* s1, const char* s2, size_t n, locale_t locale);

__END_DECLS

#endif
