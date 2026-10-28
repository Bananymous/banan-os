#ifndef _BYTESWAP_H
#define _BYTESWAP_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#define bswap_16(x) \
	((((x) & 0xFF00) >> 8) | \
	 (((x) & 0x00FF) << 8))

#define bswap_32(x) \
	((((x) & 0xFF000000) >> 24) | \
	 (((x) & 0x00FF0000) >>  8) | \
	 (((x) & 0x0000FF00) <<  8) | \
	 (((x) & 0x000000FF) << 24))

#define bswap_64(x) \
	((((x) & 0xFF00000000000000) >> 56) | \
	 (((x) & 0x00FF000000000000) >> 40) | \
	 (((x) & 0x0000FF0000000000) >> 24) | \
	 (((x) & 0x000000FF00000000) >>  8) | \
	 (((x) & 0x00000000FF000000) <<  8) | \
	 (((x) & 0x0000000000FF0000) << 24) | \
	 (((x) & 0x000000000000FF00) << 40) | \
	 (((x) & 0x00000000000000FF) << 56))

__END_DECLS

#endif
