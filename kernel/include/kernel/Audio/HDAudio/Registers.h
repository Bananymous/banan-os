#pragma once

#include <stdint.h>

namespace Kernel::HDAudio
{

	enum Regs : uint8_t
	{
		GCAP = 0x00,
		VMIN = 0x02,
		VMAJ = 0x03,
		GCTL = 0x08,

		INTCTL = 0x20,
		INTSTS = 0x24,

		CORBLBASE = 0x40,
		CORBUBASE = 0x44,
		CORBWP    = 0x48,
		CORBRP    = 0x4A,
		CORBCTL   = 0x4C,
		CORBSTS   = 0x4D,
		CORBSIZE  = 0x4E,

		RIRBLBASE = 0x50,
		RIRBUBASE = 0x54,
		RIRBWP    = 0x58,
		RINTCNT   = 0x5A,
		RIRBCTL   = 0x5C,
		RIRBSTS   = 0x5D,
		RIRBSIZE  = 0x5E,

		ICOI = 0x60,
		ICII = 0x64,
		ICIS = 0x68,

		SDCTL   = 0x00,
		SDSTS   = 0x03,
		SDLPIB  = 0x04,
		SDCBL   = 0x08,
		SDLVI   = 0x0C,
		SDFIFOD = 0x10,
		SDFMT   = 0x12,
		SDBDPL  = 0x18,
		SDBDPU  = 0x1C,
	};

}
