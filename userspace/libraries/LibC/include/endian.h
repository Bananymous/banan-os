#ifndef _ENDIAN_H
#define _ENDIAN_H 1

// https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/endian.h.html

#include <sys/cdefs.h>

#include <stdint.h>

#define LITTLE_ENDIAN 0
#define BIG_ENDIAN 1

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define BYTE_ORDER LITTLE_ENDIAN
#endif

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define BYTE_ORDER BIG_ENDIAN
#endif

__BEGIN_DECLS

uint16_t be16toh(uint16_t);
uint32_t be32toh(uint32_t);
uint64_t be64toh(uint64_t);

uint16_t htobe16(uint16_t);
uint32_t htobe32(uint32_t);
uint64_t htobe64(uint64_t);

uint16_t htole16(uint16_t);
uint32_t htole32(uint32_t);
uint64_t htole64(uint64_t);

uint16_t le16toh(uint16_t);
uint32_t le32toh(uint32_t);
uint64_t le64toh(uint64_t);

__END_DECLS

#endif
