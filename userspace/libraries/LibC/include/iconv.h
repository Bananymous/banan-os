#ifndef _ICONV_H
#define _ICONV_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/iconv.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_size_t
#include <sys/types.h>

typedef void* iconv_t;

size_t	iconv(iconv_t cd, char** __restrict inbuf, size_t* __restrict inbytesleft, char** __restrict outbuf, size_t* __restrict outbytesleft);
int		iconv_close(iconv_t cd);
iconv_t	iconv_open(const char* tocode, const char* fromcode);

__END_DECLS

#endif
