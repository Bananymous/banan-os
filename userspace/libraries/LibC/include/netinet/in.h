#ifndef _NETINET_IN_H
#define _NETINET_IN_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/netinet_in.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <bits/inet_common.h>
#include <sys/socket.h>

#define IPPROTO_IP   1
#define IPPROTO_IPV6 2
#define IPPROTO_ICMP 3
#define IPPROTO_RAW  4
#define IPPROTO_TCP  5
#define IPPROTO_UDP  6

enum
{
	IP_ADD_MEMBERSHIP,
#define IP_ADD_MEMBERSHIP IP_ADD_MEMBERSHIP
	IP_ADD_SOURCE_MEMBERSHIP,
#define IP_ADD_SOURCE_MEMBERSHIP IP_ADD_SOURCE_MEMBERSHIP
	IP_DROP_MEMBERSHIP,
#define IP_DROP_MEMBERSHIP IP_DROP_MEMBERSHIP
	IP_DROP_SOURCE_MEMBERSHIP,
#define IP_DROP_SOURCE_MEMBERSHIP IP_DROP_SOURCE_MEMBERSHIP
	IP_MULTICAST_IF,
#define IP_MULTICAST_IF IP_MULTICAST_IF
	IP_MULTICAST_LOOP,
#define IP_MULTICAST_LOOP IP_MULTICAST_LOOP
	IP_MULTICAST_TTL,
#define IP_MULTICAST_TTL IP_MULTICAST_TTL
	IP_TTL,
#define IP_TTL IP_TTL
};

enum
{
	IPV6_ADD_MEMBERSHIP,
#define IPV6_ADD_MEMBERSHIP IPV6_ADD_MEMBERSHIP
	IPV6_DROP_MEMBERSHIP,
#define IPV6_DROP_MEMBERSHIP IPV6_DROP_MEMBERSHIP
	IPV6_JOIN_GROUP,
#define IPV6_JOIN_GROUP IPV6_JOIN_GROUP
	IPV6_LEAVE_GROUP,
#define IPV6_LEAVE_GROUP IPV6_LEAVE_GROUP
	IPV6_MULTICAST_HOPS,
#define IPV6_MULTICAST_HOPS IPV6_MULTICAST_HOPS
	IPV6_MULTICAST_IF,
#define IPV6_MULTICAST_IF IPV6_MULTICAST_IF
	IPV6_MULTICAST_LOOP,
#define IPV6_MULTICAST_LOOP IPV6_MULTICAST_LOOP
	IPV6_UNICAST_HOPS,
#define IPV6_UNICAST_HOPS IPV6_UNICAST_HOPS
	IPV6_V6ONLY,
#define IPV6_V6ONLY IPV6_V6ONLY
};

#define INADDR_ANY			0
#define INADDR_NONE			0xFFFFFFFF
#define INADDR_BROADCAST	0xFFFFFFFF
#define INADDR_LOOPBACK     0x7F000001

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

struct sockaddr_in
{
	sa_family_t		sin_family;	/* AF_INET. */
	in_port_t		sin_port;	/* Port number. */
	struct in_addr	sin_addr;	/* IP address. */
	unsigned char	sin_zero[8];
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

struct ip_mreq
{
	struct in_addr imr_multiaddr; /* IP multicast group address. */
	struct in_addr imr_interface; /* IP address of local interface. */
};

struct ip_mreq_source
{
	struct in_addr imr_multiaddr;  /* IP multicast group address. */
	struct in_addr imr_interface;  /* IP address of local interface. */
	struct in_addr imr_sourceaddr; /* IP address of multicast source. */
};

__END_DECLS

#endif
