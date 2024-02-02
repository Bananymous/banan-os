#pragma once

#include <BAN/Formatter.h>

namespace BAN
{

	struct MACAddress
	{
		uint8_t address[6];
	};

}

namespace BAN::Formatter
{

	template<typename F>
	void print_argument(F putc, const MACAddress& mac, const ValueFormat&)
	{
		ValueFormat format {
			.base = 16,
			.percision = 0,
			.fill = 2,
			.upper = true,
		};

		print_argument(putc, mac.address[0], format);
		for (size_t i = 1; i < 6; i++)
		{
			putc(':');
			print_argument(putc, mac.address[i], format);
		}
	}

}
