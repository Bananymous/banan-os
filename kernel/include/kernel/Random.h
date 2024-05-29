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
		template<typename T>
		static T get();
	};

	template<>
	inline uint32_t Random::get<uint32_t>() { return Random::get_u32(); }

	template<>
	inline uint64_t Random::get<uint64_t>() { return Random::get_u64(); }

}
