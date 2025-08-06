#ifndef _IFADDRS_H
#define _IFADDRS_H 1

// https://man7.org/linux/man-pages/man3/getifaddrs.3.html

#include <sys/cdefs.h>

__BEGIN_DECLS

struct ifaddrs
{
	struct ifaddrs*  ifa_next;    /* Next item in list */
	char*            ifa_name;    /* Name of interface */
	unsigned int     ifa_flags;   /* Flags from SIOCGIFFLAGS */
	struct sockaddr* ifa_addr;    /* Address of interface */
	struct sockaddr* ifa_netmask; /* Netmask of interface */
	union {
		struct sockaddr* ifu_broadaddr; /* Broadcast address of interface */
		struct sockaddr* ifu_dstaddr;   /* Point-to-point destination address */
	} ifa_ifu;
	void*            ifa_data;    /* Address-specific data */
};

#define ifa_broadaddr ifa_ifu.ifu_broadaddr
#define ifa_dstaddr   ifa_ifu.ifu_dstaddr

int getifaddrs(struct ifaddrs **ifap);
void freeifaddrs(struct ifaddrs *ifa);

__END_DECLS

#endif
