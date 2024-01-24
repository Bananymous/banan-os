#pragma once

#include <BAN/Traits.h>

#include <stddef.h>
#include <stdint.h>

namespace BAN::Math
{

	template<typename T>
	inline constexpr T abs(T val)
	{
		return val < 0 ? -val : val;
	}

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

	template<typename T>
	requires is_same_v<T, unsigned int> || is_same_v<T, unsigned long> || is_same_v<T, unsigned long long>
	inline constexpr T ilog2(T value)
	{
		if constexpr(is_same_v<T, unsigned int>)
			return sizeof(T) * 8 - __builtin_clz(value) - 1;
		if constexpr(is_same_v<T, unsigned long>)
			return sizeof(T) * 8 - __builtin_clzl(value) - 1;
		return sizeof(T) * 8 - __builtin_clzll(value) - 1;
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

}
