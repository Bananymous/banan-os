#ifndef _SYS_UN_H
#define _SYS_UN_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_un.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <bits/types/sa_family_t.h>

struct sockaddr_un
{
	sa_family_t	sun_family;	/* Address family. */
	char		sun_path[];	/* Socket pathname. */
};

__END_DECLS

#endif
