#pragma once

#include <stdint.h>

union GateDescriptor
{
	struct
	{
		uint16_t	offset_lo;
		uint16_t	selector;
		uint8_t		reserved;
		uint8_t		type		: 4;
		uint8_t		zero		: 1;
		uint8_t		dpl			: 2;
		uint8_t		present		: 1;
		uint16_t	offset_hi;
	};

	struct
	{
		uint32_t low;
		uint32_t high;
	};
	
} __attribute__((packed));

void idt_initialize();