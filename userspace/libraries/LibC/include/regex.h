#ifndef _REGEX_H
#define _REGEX_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/regex.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_size_t
#include <sys/types.h>

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
	_re_literal,
	_re_group,
	_re_anchor_begin,
	_re_anchor_end,
	_re_group_tag1,
	_re_group_tag2,
} _regex_elem_e;

typedef struct
{
	int min;
	int max;
} _regex_qualifier_t;

typedef struct _regex_elem_t
{
	_regex_elem_e type;
	_regex_qualifier_t qualifier;
	union
	{
		uint32_t literal[0x100 / 32];
		size_t group_tag;
		struct {
			struct _regex_elem_t* elements;
			size_t elements_len;
			size_t index;
		} group;
	} as;
} _regex_elem_t;

typedef struct
{
	size_t re_nsub;
	_regex_elem_t* _compiled;
	size_t _compiled_len;
	int _cflags;
} regex_t;

typedef __PTRDIFF_TYPE__ regoff_t;

typedef struct
{
	regoff_t rm_so;	/* Byte offset from start of string to start of substring. */
	regoff_t rm_eo;	/* Byte offset from start of string of the first character after the end of substring. */
} regmatch_t;

#define REG_EXTENDED	0x1
#define REG_ICASE		0x2
#define REG_NOSUB		0x4
#define REG_NEWLINE		0x8

#define REG_NOTBOL    0x1
#define REG_NOTEOL    0x2

#define REG_NOMATCH   1
#define REG_BADPAT    2
#define REG_ECOLLATE  3
#define REG_ECTYPE    4
#define REG_EESCAPE   5
#define REG_ESUBREG   6
#define REG_EBRACK    7
#define REG_EPAREN    8
#define REG_EBRACE    9
#define REG_BADBR    10
#define REG_ERANGE   11
#define REG_ESPACE   12
#define REG_BADRPT   13

int		regcomp(regex_t* __restrict preg, const char* __restrict pattern, int cflags);
size_t	regerror(int errcode, const regex_t* __restrict preg, char* __restrict errbuf, size_t errbuf_size);
int		regexec(const regex_t* __restrict preg, const char* __restrict string, size_t nmatch, regmatch_t pmatch[], int eflags);
void	regfree(regex_t* preg);

__END_DECLS

#endif
