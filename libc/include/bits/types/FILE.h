#ifndef _BITS_TYPES_FILE_H
#define _BITS_TYPES_FILE_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/stdio.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#ifndef __FILE_defined
	#define __FILE_defined 1
	struct FILE;
	typedef struct FILE FILE;
#endif

__END_DECLS

#endif
