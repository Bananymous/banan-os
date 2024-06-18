#ifndef _MONETARY_H
#define _MONETARY_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/monetary.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <bits/types/locale_t.h>

#define __need_size_t
#include <stddef.h>

#define __need_ssize_t
#include <sys/types.h>

ssize_t strfmon(char* __restrict s, size_t maxsize, const char* __restrict format, ...);
ssize_t strfmon_l(char* __restrict s, size_t maxsize, locale_t locale, const char* __restrict format, ...);

__END_DECLS

#endif
