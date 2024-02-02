#pragma once

#include <BAN/Formatter.h>

namespace BAN
{

	struct IPv4Address
	{
		IPv4Address(uint32_t u32_address)
		{
			address[0] = u32_address >> 24;
			address[1] = u32_address >> 16;
			address[2] = u32_address >>  8;
			address[3] = u32_address >>  0;
		}

		constexpr uint32_t as_u32()
		{
			return
				((uint32_t)address[0] << 24) |
				((uint32_t)address[1] << 16) |
				((uint32_t)address[2] <<  8) |
				((uint32_t)address[3] <<  0);
		}

		uint8_t address[4];
	};
	static_assert(sizeof(IPv4Address) == 4);

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

		print_argument(putc, ipv4.address[0], format);
		for (size_t i = 1; i < 4; i++)
		{
			putc('.');
			print_argument(putc, ipv4.address[i], format);
		}
	}

}
