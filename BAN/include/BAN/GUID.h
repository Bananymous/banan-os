#pragma once

#include <BAN/Optional.h>
#include <BAN/StringView.h>

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
	};
	static_assert(sizeof(GUID) == 16);

}
