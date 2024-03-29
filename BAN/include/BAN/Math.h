#pragma once

#include <BAN/Traits.h>

#include <stddef.h>
#include <stdint.h>

namespace BAN::Math
{

	template<typename T>
	inline constexpr T min(T a, T b)
	{
		return a < b ? a : b;
	}

	template<typename T>
	inline constexpr T max(T a, T b)
	{
		return a > b ? a : b;
	}

	template<typename T>
	inline constexpr T clamp(T x, T min, T max)
	{
		return x < min ? min : x > max ? max : x;
	}

	template<integral T>
	inline constexpr T gcd(T a, T b)
	{
		T t;
		while (b)
		{
			t = b;
			b = a % b;
			a = t;
		}
		return a;
	}

	template<integral T>
	inline constexpr T lcm(T a, T b)
	{
		return a / gcd(a, b) * b;
	}

	template<integral T>
	inline constexpr T div_round_up(T a, T b)
	{
		return (a + b - 1) / b;
	}

	template<integral T>
	inline constexpr bool is_power_of_two(T value)
	{
		if (value == 0)
			return false;
		return (value & (value - 1)) == 0;
	}

	template<floating_point T>
	inline constexpr T log2(T value)
	{
		T result;
		asm volatile("fyl2x" : "=t"(result) : "0"(value), "u"((T)1.0) : "st(1)");
		return result;
	}

	template<floating_point T>
	inline constexpr T log10(T value)
	{
		constexpr T INV_LOG_2_10 = 0.3010299956639811952137388947244930267681898814621085413104274611;
		T result;
		asm volatile("fyl2x" : "=t"(result) : "0"(value), "u"(INV_LOG_2_10) : "st(1)");
		return result;
	}

	template<floating_point T>
	inline constexpr T log(T value, T base)
	{
		return log2(value) / log2(base);
	}

	template<floating_point T>
	inline constexpr T pow(T base, T exp)
	{
		T result;
		asm volatile(
			"fyl2x;"
			"fld1;"
			"fld %%st(1);"
			"fprem;"
			"f2xm1;"
			"faddp;"
			"fscale;"
			"fxch %%st(1);"
			"fstp %%st;"
			: "=t"(result)
			: "0"(base), "u"(exp)
		);
		return result;
	}

	template<integral T>
	inline constexpr T little_endian_to_host(const uint8_t* bytes)
	{
		T result = 0;
		for (size_t i = 0; i < sizeof(T); i++)
			result |= (T)bytes[i] << (i * 8);
		return result;
	}

	template<integral T>
	inline constexpr T big_endian_to_host(const uint8_t* bytes)
	{
		T result = 0;
		for (size_t i = 0; i < sizeof(T); i++)
			result |= (T)bytes[i] << (8 * (sizeof(T) - i - 1));
		return result;
	}

}