#pragma once

#include <stdint.h>

constexpr uint8_t IRQ_VECTOR_BASE = 0x20;

namespace IDT
{

	void initialize();
	void register_irq_handler(uint8_t irq, void(*f)());

}