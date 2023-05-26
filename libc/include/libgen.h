#ifndef _LIBGEN_H
#define _LIBGEN_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/ligen.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

char* basename(char* path);
char* dirname(char* path);

__END_DECLS

#endif
