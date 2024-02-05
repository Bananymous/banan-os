#pragma once

#include <BAN/Endianness.h>
#include <BAN/Formatter.h>
#include <BAN/Hash.h>

namespace BAN
{

	struct IPv4Address
	{
		constexpr IPv4Address(uint32_t u32_address)
		{
			raw = u32_address;
		}

		constexpr IPv4Address(uint8_t oct1, uint8_t oct2, uint8_t oct3, uint8_t oct4)
		{
			octets[0] = oct1;
			octets[1] = oct2;
			octets[2] = oct3;
			octets[3] = oct4;
		}

		constexpr bool operator==(const IPv4Address& other) const
		{
			return raw == other.raw;
		}

		constexpr IPv4Address mask(const IPv4Address& other) const
		{
			return IPv4Address(raw & other.raw);
		}

		union
		{
			uint8_t octets[4];
			uint32_t raw;
		} __attribute__((packed));
	};
	static_assert(sizeof(IPv4Address) == 4);

	template<>
	struct hash<IPv4Address>
	{
		constexpr hash_t operator()(IPv4Address ipv4) const
		{
			return hash<uint32_t>()(ipv4.raw);
		}
	};

}

namespace BAN::Formatter
{

	template<typename F>
	void print_argument(F putc, const IPv4Address& ipv4, const ValueFormat&)
	{
		ValueFormat format {
			.base = 10,
			.percision = 0,
			.fill = 0,
			.upper = false,
		};

		print_argument(putc, ipv4.octets[0], format);
		for (size_t i = 1; i < 4; i++)
		{
			putc('.');
			print_argument(putc, ipv4.octets[i], format);
		}
	}

}
