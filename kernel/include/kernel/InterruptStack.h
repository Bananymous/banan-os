#pragma once

#include <stdint.h>

namespace Kernel
{

	struct InterruptStack
	{
		uint64_t rip;
		uint64_t cs;
		uint64_t flags;
		uint64_t rsp;
		uint64_t ss;
	};

}
