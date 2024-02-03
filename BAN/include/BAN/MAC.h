#pragma once

#include <BAN/Formatter.h>

namespace BAN
{

	struct MACAddress
	{
		uint8_t address[6];

		constexpr bool operator==(const MACAddress& other) const
		{
			return
				address[0] == other.address[0] &&
				address[1] == other.address[1] &&
				address[2] == other.address[2] &&
				address[3] == other.address[3] &&
				address[4] == other.address[4] &&
				address[5] == other.address[5];
		}
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
