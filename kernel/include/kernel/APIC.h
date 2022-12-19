#pragma once

#include <stdint.h>

namespace APIC
{

	void Initialize();
	void EOI();
	void GetISR(uint32_t[8]);
	void EnableIRQ(uint8_t irq);

}