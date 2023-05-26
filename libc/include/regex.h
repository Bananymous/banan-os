#ifndef _REGEX_H
#define _REGEX_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/regex.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_size_t
#include <sys/types.h>

typedef struct
{
	size_t re_nsub;
} regex_t;

typedef __PTRDIFF_TYPE__ regoff_t;

typedef struct
{
	regoff_t rm_so;	/* Byte offset from start of string to start of substring. */
	regoff_t rm_eo;	/* Byte offset from start of string of the first character after the end of substring. */
} regmatch_t;

#define REG_EXTENDED	0x01
#define REG_ICASE		0x02
#define REG_NOSUB		0x04
#define REG_NEWLINE		0x80

#define REG_NOTBOL		0x0001
#define REG_NOTEOL		0x0002
#define REG_NOMATCH		0x0004
#define REG_BADPAT		0x0008
#define REG_ECOLLATE	0x0010
#define REG_ECTYPE		0x0020
#define REG_EESCAPE		0x0040
#define REG_ESUBREG		0x0080
#define REG_EBRACK		0x0100
#define REG_EPAREN		0x0200
#define REG_EBRACE		0x0400
#define REG_BADBR		0x0800
#define REG_ERANGE		0x1000
#define REG_ESPACE		0x2000
#define REG_BADRPT		0x4000

int		regcomp(regex_t* __restrict preg, const char* __restrict pattern, int cflags);
size_t	regerror(int errcode, const regex_t* __restrict preg, char* __restrict errbuf, size_t errbuf_size);
int		regexec(const regex_t* __restrict preg, const char* __restrict string, size_t nmatch, regmatch_t pmatch[__restrict], int eflags);
void	regfree(regex_t* preg);

__END_DECLS

#endif
