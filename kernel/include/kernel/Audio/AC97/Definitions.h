#pragma once

#include <stdint.h>

namespace Kernel::AC97
{

	struct BufferDescriptorListEntry
	{
		uint32_t address;
		uint16_t samples;
		uint16_t flags; // bit 14: last entry, bit 15: IOC
	};

}
