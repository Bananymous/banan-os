#pragma once

#include <BAN/Traits.h>

#include <stdint.h>

namespace Kernel
{

	class Random
	{
	public:
		static void initialize();
		static uint32_t get_u32();
		static uint64_t get_u64();
		template<BAN::unsigned_integral T> requires (sizeof(T) == 4)
		static T get() { return Random::get_u32(); }
		template<BAN::unsigned_integral T> requires (sizeof(T) == 8)
		static T get() { return Random::get_u64(); }
	};



}
