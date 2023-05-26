#ifndef _NET_IF_H
#define _NET_IF_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/net_if.h.html

#include <sys/cdefs.h>

#define IF_NAMESIZE 16

__BEGIN_DECLS

struct if_nameindex
{
	unsigned if_index;	/* Numeric index of the interface. */
	char* if_name;		/* Null-terminated name of the interface. */
};

void					if_freenameindex(struct if_nameindex* ptr);
char*					if_indextoname(unsigned ifindex, char* ifname);
struct if_nameindex*	if_nameindex(void);
unsigned				if_nametoindex(const char* ifname);

__END_DECLS

#endif
