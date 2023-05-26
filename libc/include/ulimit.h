#ifndef _ULIMIT_H
#define _ULIMIT_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/ulimit.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define UL_GETFSIZE 0
#define UL_SETFSIZE 1

long ulimit(int, ...);

__END_DECLS

#endif
