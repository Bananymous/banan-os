#pragma once

#include <stdint.h>

namespace Kernel
{

	struct InterruptStack
	{
		uintptr_t ip;
		uintptr_t cs;
		uintptr_t flags;
		uintptr_t sp;
		uintptr_t ss;
	};

}
