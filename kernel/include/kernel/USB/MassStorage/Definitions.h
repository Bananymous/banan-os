#pragma once

#include <stdint.h>

namespace Kernel::USBMassStorage
{

	struct CBW
	{
		uint32_t dCBWSignature;
		uint32_t dCBWTag;
		uint32_t dCBWDataTransferLength;
		uint8_t bmCBWFlags;
		uint8_t bCBWLUN;
		uint8_t bCBWCBLength;
		uint8_t CBWCB[16];
	} __attribute__((packed));
	static_assert(sizeof(CBW) == 31);

	struct CSW
	{
		uint32_t dCSWSignature;
		uint32_t dCSWTag;
		uint32_t dCSWDataResidue;
		uint8_t bmCSWStatus;
	} __attribute__((packed));
	static_assert(sizeof(CSW) == 13);

}
