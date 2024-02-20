#include <kernel/CPUID.h>
#include <kernel/Debug.h>
#include <kernel/Random.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{


	// Constants and algorithm from https://en.wikipedia.org/wiki/Permuted_congruential_generator

	static uint64_t s_rand_seed = 0x4d595df4d0f33173;
	static constexpr uint64_t s_rand_multiplier = 6364136223846793005;
	static constexpr uint64_t s_rand_increment = 1442695040888963407;

	void Random::initialize()
	{
		uint32_t ecx, edx;
		CPUID::get_features(ecx, edx);

		if (ecx & CPUID::ECX_RDRND)
		{
			asm volatile("rdrand %0" : "=a"(s_rand_seed));
			dprintln("RNG seeded by RDRAND");
		}
		else
		{
			auto rt = SystemTimer::get().real_time();
			s_rand_seed ^= rt.tv_sec ^ rt.tv_nsec;
			dprintln("RNG seeded by real time");
		}
	}

	uint32_t Random::get_u32()
	{
		auto rotr32 = [](uint32_t x, unsigned r) { return x >> r | x << (-r & 31); };

		uint64_t x = s_rand_seed;
		unsigned count = (unsigned)(x >> 59);

		s_rand_seed = x * s_rand_multiplier + s_rand_increment;
		x ^= x >> 18;

		return rotr32(x >> 27, count) % UINT32_MAX;
	}

	uint64_t Random::get_u64()
	{
		return ((uint64_t)get_u32() << 32) | get_u32();
	}

}
