#pragma once

#include <BAN/ByteSpan.h>

namespace LibDEFLATE
{

	inline uint32_t calculate_adler32(BAN::ConstByteSpan data)
	{
		uint32_t s1 = 1;
		uint32_t s2 = 0;

		for (size_t i = 0; i < data.size(); i++)
		{
			s1 = (s1 + data[i]) % 65521;
			s2 = (s2 + s1)      % 65521;
		}

		return (s2 << 16) | s1;
	}

	inline uint32_t calculate_crc32(BAN::ConstByteSpan data)
	{
		uint32_t crc32 = 0xFFFFFFFF;
		uint32_t polynomial = 0xEDB88320;

		for (size_t i = 0; i < data.size(); i++) {
			crc32 ^= data[i];

			for (size_t j = 0; j < 8; j++) {
				if (crc32 & 1)
					crc32 = (crc32 >> 1) ^ polynomial;
				else
					crc32 >>= 1;
			}
		}

		return ~crc32;
	}

	inline constexpr uint16_t reverse_bits(uint16_t value, size_t count)
	{
		uint16_t reverse = 0;
		for (uint8_t bit = 0; bit < count; bit++)
			reverse |= ((value >> bit) & 1) << (count - bit - 1);
		return reverse;
	}

}
