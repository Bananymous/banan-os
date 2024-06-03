#pragma once

#include <stddef.h>
#include <stdint.h>

namespace BAN::UTF8
{

	static constexpr uint32_t invalid = 0xFFFFFFFF;

	constexpr uint32_t byte_length(uint8_t first_byte)
	{
		if ((first_byte & 0x80) == 0x00)
			return 1;
		if ((first_byte & 0xE0) == 0xC0)
			return 2;
		if ((first_byte & 0xF0) == 0xE0)
			return 3;
		if ((first_byte & 0xF8) == 0xF0)
			return 4;
		return 0;
	}

	template<typename T> requires (sizeof(T) == 1)
	constexpr uint32_t to_codepoint(const T* bytes)
	{
		uint32_t length = byte_length(bytes[0]);

		for (uint32_t i = 1; i < length; i++)
			if (((uint8_t)bytes[i] & 0xC0) != 0x80)
				return UTF8::invalid;

		switch (length)
		{
			case 1: return (((uint8_t)bytes[0] & 0x80) != 0x00) ? UTF8::invalid :   (uint8_t)bytes[0];
			case 2: return (((uint8_t)bytes[0] & 0xE0) != 0xC0) ? UTF8::invalid : (((uint8_t)bytes[0] & 0x1F) <<  6) |  ((uint8_t)bytes[1] & 0x3F);
			case 3: return (((uint8_t)bytes[0] & 0xF0) != 0xE0) ? UTF8::invalid : (((uint8_t)bytes[0] & 0x0F) << 12) | (((uint8_t)bytes[1] & 0x3F) <<  6) |  ((uint8_t)bytes[2] & 0x3F);
			case 4: return (((uint8_t)bytes[0] & 0xF8) != 0xF0) ? UTF8::invalid : (((uint8_t)bytes[0] & 0x07) << 18) | (((uint8_t)bytes[1] & 0x3F) << 12) | (((uint8_t)bytes[2] & 0x3F) << 6) | ((uint8_t)bytes[3] & 0x3F);
		}

		return UTF8::invalid;
	}

	template<typename T>
	constexpr bool from_codepoints(const T* codepoints, size_t count, char* out)
	{
		uint8_t* ptr = (uint8_t*)out;

		for (size_t i = 0; i < count; i++)
		{
			if (codepoints[i] < 0x80)
			{
				*ptr++ = codepoints[i];
			}
			else if (codepoints[i] < 0x800)
			{
				*ptr++ = 0xC0 | ((codepoints[i] >> 6) & 0x1F);
				*ptr++ = 0x80 | ((codepoints[i] >> 0) & 0x3F);
			}
			else if (codepoints[i] < 0x10000)
			{
				*ptr++ = 0xE0 | ((codepoints[i] >> 12) & 0x0F);
				*ptr++ = 0x80 | ((codepoints[i] >>  6) & 0x3F);
				*ptr++ = 0x80 | ((codepoints[i] >>  0) & 0x3F);
			}
			else if (codepoints[i] < 0x110000)
			{
				*ptr++ = 0xF0 | ((codepoints[i] >> 18) & 0x07);
				*ptr++ = 0x80 | ((codepoints[i] >> 12) & 0x3F);
				*ptr++ = 0x80 | ((codepoints[i] >>  6) & 0x3F);
				*ptr++ = 0x80 | ((codepoints[i] >>  0) & 0x3F);
			}
			else
			{
				return false;
			}
		}

		return true;
	}

}
