#ifndef _NETINET_IN_H
#define _NETINET_IN_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/netinet_in.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <inttypes.h>
#include <sys/socket.h>

#define IPPROTO_IP		1
#define IPPROTO_IPV6	2
#define IPPROTO_ICMP	3
#define IPPROTO_RAW		4
#define IPPROTO_TCP		5
#define IPPROTO_UDP		6

#define IPV6_JOIN_GROUP		1
#define IPV6_LEAVE_GROUP	2
#define IPV6_MULTICAST_HOPS	3
#define IPV6_MULTICAST_IF	4
#define IPV6_MULTICAST_LOOP	5
#define IPV6_UNICAST_HOPS	6
#define IPV6_V6ONLY			7

#define INADDR_ANY			0
#define INADDR_BROADCAST	0xFFFFFFFF
#define INADDR_LOOPBACK     0x7F000001

#define INET_ADDRSTRLEN		16
#define INET6_ADDRSTRLEN	46

#define IN6ADDR_ANY_INIT		{ { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } } }
#define IN6ADDR_LOOPBACK_INIT	{ { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 } } }

#if 0
#define IN6_IS_ADDR_UNSPECIFIED(addr)
#define IN6_IS_ADDR_LOOPBACK(addr)
#define IN6_IS_ADDR_MULTICAST(addr)
#define IN6_IS_ADDR_LINKLOCAL(addr)
#define IN6_IS_ADDR_SITELOCAL(addr)
#define IN6_IS_ADDR_V4MAPPED(addr)
#define IN6_IS_ADDR_V4COMPAT(addr)
#define IN6_IS_ADDR_MC_NODELOCAL(addr)
#define IN6_IS_ADDR_MC_LINKLOCAL(addr)
#define IN6_IS_ADDR_MC_SITELOCAL(addr)
#define IN6_IS_ADDR_MC_ORGLOCAL(addr)
#define IN6_IS_ADDR_MC_GLOBAL(addr)
#endif

typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

struct in_addr
{
	in_addr_t s_addr;
};

struct sockaddr_in
{
	sa_family_t		sin_family;	/* AF_INET. */
	in_port_t		sin_port;	/* Port number. */
	struct in_addr	sin_addr;	/* IP address. */
};

struct in6_addr
{
	uint8_t s6_addr[16];
};

struct sockaddr_in6
{
	sa_family_t		sin6_family;	/* AF_INET6. */
	in_port_t		sin6_port;		/* Port number. */
	uint32_t		sin6_flowinfo;	/* IPv6 traffic class and flow information. */
	struct in6_addr	sin6_addr;		/* IPv6 address. */
	uint32_t		sin6_scope_id;	/* Set of interfaces for a scope. */
};

extern const struct in6_addr in6addr_any;
extern const struct in6_addr in6addr_loopback;

struct ipv6_mreq
{
	struct in6_addr	ipv6mr_multiaddr;	/* IPv6 multicast address. */
	unsigned		ipv6mr_interface;	/* Interface index. */
};

__END_DECLS

#endif
