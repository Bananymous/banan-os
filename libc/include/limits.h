#ifndef _LIMITS_H
#define _LIMITS_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/limits.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

// FIXME: What do I have to define here?
//        glibc seems to only define numerical
//        and posix constants

#define OPEN_MAX 64
#define NAME_MAX 255

__END_DECLS

#endif
