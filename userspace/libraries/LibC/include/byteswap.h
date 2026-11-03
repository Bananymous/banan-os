#ifndef _BYTESWAP_H
#define _BYTESWAP_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <stdint.h>

#define bswap_16(x) \
	((((uint16_t)(x) & 0xFF00) >> 8) | \
	 (((uint16_t)(x) & 0x00FF) << 8))

#define bswap_32(x) \
	((((uint32_t)(x) & 0xFF000000) >> 24) | \
	 (((uint32_t)(x) & 0x00FF0000) >>  8) | \
	 (((uint32_t)(x) & 0x0000FF00) <<  8) | \
	 (((uint32_t)(x) & 0x000000FF) << 24))

#define bswap_64(x) \
	((((uint64_t)(x) & 0xFF00000000000000) >> 56) | \
	 (((uint64_t)(x) & 0x00FF000000000000) >> 40) | \
	 (((uint64_t)(x) & 0x0000FF0000000000) >> 24) | \
	 (((uint64_t)(x) & 0x000000FF00000000) >>  8) | \
	 (((uint64_t)(x) & 0x00000000FF000000) <<  8) | \
	 (((uint64_t)(x) & 0x0000000000FF0000) << 24) | \
	 (((uint64_t)(x) & 0x000000000000FF00) << 40) | \
	 (((uint64_t)(x) & 0x00000000000000FF) << 56))

__END_DECLS

#endif
