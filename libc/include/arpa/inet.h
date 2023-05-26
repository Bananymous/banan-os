#ifndef _INET_H
#define _INET_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/arpa_inet.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <netinet/in.h>
#include <inttypes.h>

uint32_t htonl(uint32_t);
uint16_t htons(uint16_t);
uint32_t ntohl(uint32_t);
uint16_t ntohs(uint16_t);

in_addr_t	inet_addr(const char* cp);
char*		inet_ntoa(struct in_addr in);
const char*	inet_ntop(int af, const void* __restrict src, char* __restrict dst, socklen_t size);
int			inet_pton(int af, const char* __restrict src, void* __restrict dst);

__END_DECLS

#endif
