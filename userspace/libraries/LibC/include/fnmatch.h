#ifndef _FNMATCH_H
#define _FNMATCH_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/fnmatch.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define FNM_NOMATCH		1
#define FNM_PATHNAME	0x01
#define FNM_PERIOD		0x02
#define FNM_NOESCAPE	0x04

int fnmatch(const char* pattern, const char* string, int flags);

__END_DECLS

#endif
