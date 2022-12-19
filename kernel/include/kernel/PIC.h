#pragma once

#include <stdint.h>

namespace PIC
{

	void Remap();
	void MaskAll();
	void EOI(uint8_t);
	void Unmask(uint8_t);
	void Mask(uint8_t);

	uint16_t GetISR();

}