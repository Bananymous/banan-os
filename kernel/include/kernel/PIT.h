#pragma once

#include <stdint.h>

#define PIT_IRQ 0

namespace PIT
{

	uint64_t ms_since_boot();
	void initialize();
	void sleep(uint64_t);

}