#pragma once

#include <BAN/Optional.h>
#include <BAN/String.h>

#include <string.h>

namespace BAN
{

	struct GUID
	{
		uint32_t component1		{ 0 };
		uint16_t component2		{ 0 };
		uint16_t component3		{ 0 };
		uint8_t component45[8]	{   };

		bool operator==(const GUID& other) const
		{
			return memcmp(this, &other, sizeof(GUID)) == 0;
		}

		BAN::ErrorOr<BAN::String> to_string() const
		{
			char buffer[37];
			char* ptr = buffer;

			const auto append_hex_nibble =
				[&ptr](uint8_t nibble)
				{
					if (nibble < 10)
						*ptr++ = '0' + nibble;
					else
						*ptr++ = 'A' + nibble - 10;
				};

			const auto append_hex_byte =
				[&append_hex_nibble](uint8_t byte)
				{
					append_hex_nibble(byte >> 4);
					append_hex_nibble(byte & 0xF);
				};

			append_hex_byte((component1 >> 24) & 0xFF);
			append_hex_byte((component1 >> 16) & 0xFF);
			append_hex_byte((component1 >>  8) & 0xFF);
			append_hex_byte((component1 >>  0) & 0xFF);
			*ptr++ = '-';
			append_hex_byte((component2 >>  8) & 0xFF);
			append_hex_byte((component2 >>  0) & 0xFF);
			*ptr++ = '-';
			append_hex_byte((component3 >>  8) & 0xFF);
			append_hex_byte((component3 >>  0) & 0xFF);
			*ptr++ = '-';
			append_hex_byte(component45[0]);
			append_hex_byte(component45[1]);
			*ptr++ = '-';
			append_hex_byte(component45[2]);
			append_hex_byte(component45[3]);
			append_hex_byte(component45[4]);
			append_hex_byte(component45[5]);
			append_hex_byte(component45[6]);
			append_hex_byte(component45[7]);
			*ptr = '\0';

			BAN::String guid;
			TRY(guid.append(buffer));
			return BAN::move(guid);
		}
	};
	static_assert(sizeof(GUID) == 16);

}
