#pragma once

#include <stdint.h>

namespace PIC
{

	void initialize();
	void eoi(uint8_t);
	void unmask(uint8_t);
	void mask(uint8_t);

	uint16_t get_isr();

}