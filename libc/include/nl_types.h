#ifndef _NL_TYPES_H
#define _NL_TYPES_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/nl_types.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

typedef void* nl_catd;
typedef int nl_item;

#define NL_SETD			1
#define NL_CAT_LOCALE	1

int		charclose(nl_catd catd);
char*	catgets(nl_catd catd, int set_id, int msg_id, const char* s);
nl_catd	catopen(const char* name, int oflag);

__END_DECLS

#endif
