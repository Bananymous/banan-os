#pragma once

#include <BAN/Traits.h>

#include <stddef.h>

namespace BAN
{

	template<integral T>
	constexpr T swap_endianness(T value)
	{
		if constexpr(sizeof(T) == 1)
			return value;
		if constexpr(sizeof(T) == 2)
			return (((value >> 8) & 0xFF) << 0)
				 | (((value >> 0) & 0xFF) << 8);
		if constexpr(sizeof(T) == 4)
			return (((value >> 24) & 0xFF) <<  0)
				 | (((value >> 16) & 0xFF) <<  8)
				 | (((value >>  8) & 0xFF) << 16)
				 | (((value >>  0) & 0xFF) << 24);
		if constexpr(sizeof(T) == 8)
			return (((value >> 56) & 0xFF) <<  0)
				 | (((value >> 48) & 0xFF) <<  8)
				 | (((value >> 40) & 0xFF) << 16)
				 | (((value >> 32) & 0xFF) << 24)
				 | (((value >> 24) & 0xFF) << 32)
				 | (((value >> 16) & 0xFF) << 40)
				 | (((value >>  8) & 0xFF) << 48)
				 | (((value >>  0) & 0xFF) << 56);
		T result { 0 };
		for (size_t i = 0; i < sizeof(T); i++)
			result |= ((value >> (i * 8)) & 0xFF) << ((sizeof(T) - i - 1) * 8);
		return result;
	}

	template<integral T>
	constexpr T host_to_little_endian(T value)
	{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		return value;
#else
		return swap_endianness(value);
#endif
	}

	template<integral T>
	constexpr T little_endian_to_host(T value)
	{
		return host_to_little_endian(value);
	}

	template<integral T>
	constexpr T host_to_big_endian(T value)
	{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		return value;
#else
		return swap_endianness(value);
#endif
	}

	template<integral T>
	constexpr T big_endian_to_host(T value)
	{
		return host_to_big_endian(value);
	}

	template<integral T>
	struct LittleEndian
	{
		constexpr LittleEndian(T value)
		{
			raw = host_to_little_endian(value);
		}

		constexpr operator T() const
		{
			return host_to_little_endian(raw);
		}
	private:
		T raw;
	};

	template<integral T>
	struct BigEndian
	{
		constexpr BigEndian(T value)
		{
			raw = host_to_big_endian(value);
		}

		constexpr operator T() const
		{
			return host_to_big_endian(raw);
		}
	private:
		T raw;
	};

	template<integral T>
	using NetworkEndian = BigEndian<T>;

	template<integral T>
	constexpr T host_to_network_endian(T value)
	{
		return host_to_big_endian(value);
	}

	template<integral T>
	constexpr T network_endian_to_host(T value)
	{
		return big_endian_to_host(value);
	}

}
