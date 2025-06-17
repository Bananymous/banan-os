#include <BAN/Endianness.h>

#include <endian.h>

#define BE_TO_H(size) \
	uint##size##_t be##size##toh(uint##size##_t x) { return BAN::big_endian_to_host(x); }
BE_TO_H(16)
BE_TO_H(32)
BE_TO_H(64)
#undef BE_TO_H

#define H_TO_BE(size) \
	uint##size##_t htobe##size(uint##size##_t x) { return BAN::host_to_big_endian(x); }
H_TO_BE(16)
H_TO_BE(32)
H_TO_BE(64)
#undef H_TO_BE

#define LE_TO_H(size) \
	uint##size##_t le##size##toh(uint##size##_t x) { return BAN::little_endian_to_host(x); }
LE_TO_H(16)
LE_TO_H(32)
LE_TO_H(64)
#undef LE_TO_H

#define H_TO_LE(size) \
	uint##size##_t htole##size(uint##size##_t x) { return BAN::host_to_little_endian(x); }
H_TO_LE(16)
H_TO_LE(32)
H_TO_LE(64)
#undef H_TO_LE
