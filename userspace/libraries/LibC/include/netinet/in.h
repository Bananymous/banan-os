#ifndef _NETINET_IN_H
#define _NETINET_IN_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/netinet_in.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <bits/inet_common.h>
#include <sys/socket.h>

#define IPPROTO_IP     1
#define IPPROTO_IPV6   2
#define IPPROTO_ICMP   3
#define IPPROTO_ICMPV6 4
#define IPPROTO_RAW    5
#define IPPROTO_TCP    6
#define IPPROTO_UDP    7

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
	IP_TOS,
#define IP_TOS IP_TOS
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

#define IN_LOOPBACKNET 127
#define IN_MULTICAST(a) (((in_addr_t)(a) & 0xF0000000) == 0xE0000000)

#define INADDR_ANY			0
#define INADDR_NONE			0xFFFFFFFF
#define INADDR_BROADCAST	0xFFFFFFFF
#define INADDR_LOOPBACK     0x7F000001

#define IN6ADDR_ANY_INIT		{ { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } } }
#define IN6ADDR_LOOPBACK_INIT	{ { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 } } }

#define IN6_IS_ADDR_UNSPECIFIED(a) \
	((a)->s6_addr32[0] == 0 && \
	 (a)->s6_addr32[1] == 0 && \
	 (a)->s6_addr32[2] == 0 && \
	 (a)->s6_addr32[3] == 0)
#define IN6_IS_ADDR_LOOPBACK(a) \
	((a)->s6_addr32[0] == 0 && \
	 (a)->s6_addr32[1] == 0 && \
	 (a)->s6_addr32[2] == 0 && \
	 (a)->s6_addr32[3] == htonl(1))
#define IN6_IS_ADDR_MULTICAST(a) \
	((a)->s6_addr[0] == 0xFF)
#define IN6_IS_ADDR_LINKLOCAL(a) \
	((a)->s6_addr[0] == 0xFE && \
	((a)->s6_addr[1] & 0xC0) == 0x80)
#define IN6_IS_ADDR_SITELOCAL(a) \
	((a)->s6_addr[0] == 0xFE && \
	((a)->s6_addr[1] & 0xC0) == 0xC0)
#define IN6_IS_ADDR_V4MAPPED(a) \
	((a)->s6_addr32[0] == 0 && \
	 (a)->s6_addr32[1] == 0 && \
	 (a)->s6_addr32[2] == htonl(0x0000FFFF))
#define IN6_IS_ADDR_V4COMPAT(a) \
	((a)->s6_addr32[0] == 0 && \
	 (a)->s6_addr32[1] == 0 && \
	 (a)->s6_addr32[2] == 0 && \
	 ntohl((a)->s6_addr32[3]) > 1)
#define IN6_IS_ADDR_MC_NODELOCAL(a) \
	(IN6_IS_ADDR_MULTICAST(a) && \
	((a)->s6_addr[1] & 0x0F) == 0x01)
#define IN6_IS_ADDR_MC_LINKLOCAL(a) \
	(IN6_IS_ADDR_MULTICAST(a) && \
	((a)->s6_addr[1] & 0x0F) == 0x02)
#define IN6_IS_ADDR_MC_SITELOCAL(a) \
	(IN6_IS_ADDR_MULTICAST(a) && \
	((a)->s6_addr[1] & 0x0F) == 0x05)
#define IN6_IS_ADDR_MC_ORGLOCAL(a) \
	(IN6_IS_ADDR_MULTICAST(a) && \
	((a)->s6_addr[1] & 0x0F) == 0x08)
#define IN6_IS_ADDR_MC_GLOBAL(a) \
	(IN6_IS_ADDR_MULTICAST(a) && \
	((a)->s6_addr[1] & 0x0F) == 0x0E)

struct sockaddr_in
{
	sa_family_t		sin_family;	/* AF_INET. */
	in_port_t		sin_port;	/* Port number. */
	struct in_addr	sin_addr;	/* IP address. */
	unsigned char	sin_zero[8];
};

struct in6_addr
{
	union {
		uint8_t s6_addr[16];
		uint32_t s6_addr32[4];
	};
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
