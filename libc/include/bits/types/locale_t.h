#ifndef _BITS_LOCALE_T_H
#define _BITS_LOCALE_T_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/locale.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#ifndef __locale_t_defined
	#define __locale_t_defined 1
	typedef int locale_t;
#endif

__END_DECLS

#endif
