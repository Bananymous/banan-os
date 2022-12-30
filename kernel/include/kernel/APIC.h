#pragma once

#include <stdint.h>

namespace APIC
{

	void Initialize(bool force_pic = false);
	void EOI(uint8_t irq);
	void GetISR(uint32_t[8]);
	void EnableIRQ(uint8_t irq);

}