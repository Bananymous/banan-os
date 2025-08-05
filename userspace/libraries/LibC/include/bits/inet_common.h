#ifndef _BITS_INET_COMMON_H
#define _BITS_INET_COMMON_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <stdint.h>

#define INET_ADDRSTRLEN  16
#define INET6_ADDRSTRLEN 46

typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

struct in_addr
{
	in_addr_t s_addr;
};

uint32_t htonl(uint32_t);
uint16_t htons(uint16_t);
uint32_t ntohl(uint32_t);
uint16_t ntohs(uint16_t);

__END_DECLS

#endif
