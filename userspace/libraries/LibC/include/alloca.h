#ifndef _ALLOCA_H
#define _ALLOCA_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/stdio.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define alloca __builtin_alloca

__END_DECLS

#endif
