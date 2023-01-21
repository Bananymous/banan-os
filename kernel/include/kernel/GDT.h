#pragma once

#include <stdint.h>

namespace GDT
{

	union SegmentDesriptor
	{
		struct
		{
			uint16_t	limit_lo;
			uint16_t	base_lo;
			uint8_t		base_hi1;

			uint8_t		type			: 4;
			uint8_t		system			: 1;
			uint8_t		DPL				: 2;
			uint8_t		present			: 1;

			uint8_t		limit_hi		: 4;
			uint8_t		flags			: 4;

			uint8_t		base_hi2;
		} __attribute__((packed));
		
		struct
		{
			uint32_t low;
			uint32_t high;
		} __attribute__((packed));

	} __attribute__((packed));

	struct GDTR
	{
		uint16_t size;
		void* address;
	} __attribute__((packed));

	extern "C" GDTR g_gdtr[];
	extern "C" SegmentDesriptor g_gdt[];

}