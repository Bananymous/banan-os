#ifndef _NET_IF_H
#define _NET_IF_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/net_if.h.html

#include <sys/cdefs.h>

#include <sys/socket.h>

#define IF_NAMESIZE 16

__BEGIN_DECLS

struct if_nameindex
{
	unsigned if_index;	/* Numeric index of the interface. */
	char* if_name;		/* Null-terminated name of the interface. */
};

struct ifreq
{
	union {
		char ifrn_name[IF_NAMESIZE];
	} ifr_ifrn;

	union {
		struct sockaddr ifru_addr;
		struct sockaddr ifru_netmask;
		struct sockaddr ifru_gwaddr;
		struct sockaddr ifru_hwaddr;
		unsigned char __min_storage[sizeof(struct sockaddr) + 6];
	} ifr_ifru;
};
#define ifr_name    ifr_ifrn.ifrn_name
#define ifr_addr    ifr_ifru.ifru_addr
#define ifr_netmask ifr_ifru.ifru_addrnetmask
#define ifr_gwaddr  ifr_ifru.ifru_addrgwaddr
#define ifr_hwaddr  ifr_ifru.ifru_addrhwaddr

#define SIOCGIFADDR		1	/* Get interface address */
#define SIOCSIFADDR		2	/* Set interface address */
#define SIOCGIFNETMASK	3	/* Get network mask */
#define SIOCSIFNETMASK	4	/* Set network mask */
#define SIOCGIFGWADDR	5	/* Get gateway address */
#define SIOCSIFGWADDR	6	/* Set gateway address */
#define SIOCGIFHWADDR	7	/* Get hardware address */
#define SIOCGIFNAME		8	/* Get interface name */

void					if_freenameindex(struct if_nameindex* ptr);
char*					if_indextoname(unsigned ifindex, char* ifname);
struct if_nameindex*	if_nameindex(void);
unsigned				if_nametoindex(const char* ifname);

__END_DECLS

#endif
