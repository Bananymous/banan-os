#pragma once

#include <stdint.h>

namespace Kernel
{

	struct RSDP
	{
		uint8_t signature[8];
		uint8_t checksum;
		uint8_t oemid[6];
		uint8_t revision;
		uint32_t rsdt_address;

		// only in revision >= 2
		uint32_t length;
		uint64_t xsdt_address;
		uint8_t extended_checksum;
		uint8_t reserved[3];
	};

}
