#pragma once

#include <stdint.h>

constexpr uint8_t IRQ_VECTOR_BASE = 0x20;

namespace Kernel::IDT
{

	void initialize();
	[[noreturn]] void force_triple_fault();

}
