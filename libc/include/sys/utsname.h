#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_utsname.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

struct utsname
{
	char sysname[65];	/* Name of this implementation of the operating system. */
	char nodename[65];	/* Name of this node within the communications network to which this node is attached, if any. */
	char release[65];	/* Current release level of this implementation. */
	char version[65];	/* Current version level of this release. */
	char machine[65];	/* Name of the hardware type on which the system is running. */
};

int uname(struct utsname* name);

__END_DECLS

#endif
