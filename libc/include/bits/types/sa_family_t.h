#ifndef _BITS_TYPES_SA_FAMILY_T_H
#define _BITS_TYPES_SA_FAMILY_T_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_select.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#ifndef __sa_family_t_defined
	#define __sa_family_t_defined 1
	typedef unsigned int sa_family_t;
#endif

__END_DECLS

#endif
