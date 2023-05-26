#ifndef _GLOB_H
#define _GLOB_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/glob.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_size_t
#include <sys/types.h>

#define GLOB_APPEND		0x01
#define GLOB_DOOFFS		0x02
#define GLOB_ERR		0x04
#define GLOB_MARK		0x08
#define GLOB_NOCHECK	0x10
#define GLOB_NOESCAPE	0x20
#define GLOB_NOSORT		0x40

#define GLOB_ABORTED	1
#define GLOB_NOMATCH	2
#define GLOB_NOSPACE	3

struct glob_t
{
	size_t gl_pathc;	/* Count of paths matched by pattern. */
	char** gl_pathv;	/* Pointer to a list of matched pathnames. */
	size_t gl_offs;		/* Slots to reserve at the beginning of gl_pathv. */
};

int		glob(const char* __restrict pattern, int flags, int (*errfunc)(const char* epath, int eerrno), glob_t* __restrict pglob);
void	globfree(glob_t* pglob);

__END_DECLS

#endif
