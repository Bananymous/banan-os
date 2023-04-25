#pragma once

#include <stdint.h>

namespace Kernel::GDT
{

	void initialize();
	void set_tss_stack(uintptr_t);

}