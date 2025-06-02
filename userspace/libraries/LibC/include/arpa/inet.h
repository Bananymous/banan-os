#ifndef _ARPA_INET_H
#define _ARPA_INET_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/arpa_inet.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <bits/inet_common.h>
#include <bits/types/socklen_t.h>

in_addr_t	inet_addr(const char* cp);
int			inet_aton(const char* cp, struct in_addr* inp);
char*		inet_ntoa(struct in_addr in);
const char*	inet_ntop(int af, const void* __restrict src, char* __restrict dst, socklen_t size);
int			inet_pton(int af, const char* __restrict src, void* __restrict dst);

__END_DECLS

#endif
