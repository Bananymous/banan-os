#pragma once

#include <stdint.h>

namespace Kernel
{

	class Random
	{
	public:
		static void initialize();
		static uint32_t get_u32();
		static uint64_t get_u64();
	};

}
