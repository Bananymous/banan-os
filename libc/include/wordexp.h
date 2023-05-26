#ifndef _WORDEXP_H
#define _WORDEXP_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/wordexp.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_size_t
#include <stddef.h>

typedef struct
{
	size_t	we_wordc;	/* Count of words matched by words. */
	char**	we_wordv;	/* Pointer to list of expanded words. */
	size_t	we_offs;	/* Slots to reserve at the beginning of we_wordv. */
} wordexp_t;

#define WRDE_APPEND		0x01
#define WRDE_DOOFFS		0x02
#define WRDE_NOCMD		0x04
#define WRDE_REUSE		0x08
#define WRDE_SHOWERR	0x10
#define WRDE_UNDEF		0x20

#define WRDE_BADCHAR	1
#define WRDE_BADVAL		2
#define WRDE_CMDSUB		3
#define WRDE_NOSPACE	4
#define WRDE_SYNTAX		5

int		wordexp(const char* __restrict words, wordexp_t* __restrict pwordexp, int flags);
void	wordfree(wordexp_t* pwordexp);

__END_DECLS

#endif
