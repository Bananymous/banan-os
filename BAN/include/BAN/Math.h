#pragma once

#include <BAN/Limits.h>
#include <BAN/Numbers.h>
#include <BAN/Traits.h>

#include <float.h>

namespace BAN::Math
{

	template<typename T>
	inline constexpr T abs(T x)
	{
		return x < 0 ? -x : x;
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
	inline constexpr bool is_power_of_two(T x)
	{
		if (x == 0)
			return false;
		return (x & (x - 1)) == 0;
	}

	template<BAN::integral T>
	static constexpr bool will_multiplication_overflow(T a, T b)
	{
		if (a == 0 || b == 0)
			return false;
		if ((a > 0) == (b > 0))
			return a > BAN::numeric_limits<T>::max() / b;
		else
			return a < BAN::numeric_limits<T>::min() / b;
	}

	template<BAN::integral T>
	static constexpr bool will_addition_overflow(T a, T b)
	{
		if (a > 0 && b > 0)
			return a > BAN::numeric_limits<T>::max() - b;
		if (a < 0 && b < 0)
			return a < BAN::numeric_limits<T>::min() - b;
		return false;
	}

	template<typename T>
	requires is_same_v<T, unsigned int> || is_same_v<T, unsigned long> || is_same_v<T, unsigned long long>
	inline constexpr T ilog2(T x)
	{
		if constexpr(is_same_v<T, unsigned int>)
			return sizeof(T) * 8 - __builtin_clz(x) - 1;
		if constexpr(is_same_v<T, unsigned long>)
			return sizeof(T) * 8 - __builtin_clzl(x) - 1;
		return sizeof(T) * 8 - __builtin_clzll(x) - 1;
	}

	template<floating_point T>
	inline constexpr T floor(T x)
	{
		if constexpr(is_same_v<T, float>)
			return __builtin_floorf(x);
		if constexpr(is_same_v<T, double>)
			return __builtin_floor(x);
		if constexpr(is_same_v<T, long double>)
			return __builtin_floorl(x);
	}

	template<floating_point T>
	inline constexpr T ceil(T x)
	{
		if constexpr(is_same_v<T, float>)
			return __builtin_ceilf(x);
		if constexpr(is_same_v<T, double>)
			return __builtin_ceil(x);
		if constexpr(is_same_v<T, long double>)
			return __builtin_ceill(x);
	}

	template<floating_point T>
	inline constexpr T round(T x)
	{
		if (x == (T)0.0)
			return x;
		if (x > (T)0.0)
			return floor<T>(x + (T)0.5);
		return ceil<T>(x - (T)0.5);
	}

	template<floating_point T>
	inline constexpr T trunc(T x)
	{
		if constexpr(is_same_v<T, float>)
			return __builtin_truncf(x);
		if constexpr(is_same_v<T, double>)
			return __builtin_trunc(x);
		if constexpr(is_same_v<T, long double>)
			return __builtin_truncl(x);
	}

	template<floating_point T>
	inline constexpr T rint(T x)
	{
		asm("frndint" : "+t"(x));
		return x;
	}

	template<floating_point T>
	inline constexpr T fmod(T a, T b)
	{
		asm(
			"1:"
			"fprem;"
			"fnstsw %%ax;"
			"testb $4, %%ah;"
			"jne 1b;"
			: "+t"(a)
			: "u"(b)
			: "ax"
		);
		return a;
	}

	template<floating_point T>
	static T modf(T x, T* iptr)
	{
		const T frac = BAN::Math::fmod<T>(x, 1);
		*iptr = x - frac;
		return frac;
	}

	template<floating_point T>
	inline constexpr T frexp(T num, int* exp)
	{
		if (num == 0.0)
		{
			*exp = 0;
			return 0.0;
		}

		T _exp;
		asm("fxtract" : "+t"(num), "=u"(_exp));
		*exp = (int)_exp + 1;
		return num / (T)2.0;
	}

	template<floating_point T>
	inline constexpr T copysign(T x, T y)
	{
		if ((x < (T)0.0) != (y < (T)0.0))
			x = -x;
		return x;
	}

	namespace detail
	{

		template<floating_point T>
		inline constexpr T fyl2x(T x, T y)
		{
			asm("fyl2x" : "+t"(x) : "u"(y) : "st(1)");
			return x;
		}

	}

	template<floating_point T>
	inline constexpr T log(T x)
	{
		return detail::fyl2x<T>(x, numbers::ln2_v<T>);
	}

	template<floating_point T>
	inline constexpr T log2(T x)
	{
		return detail::fyl2x<T>(x, 1.0);
	}

	template<floating_point T>
	inline constexpr T log10(T x)
	{
		return detail::fyl2x<T>(x, numbers::lg2_v<T>);
	}

	template<floating_point T>
	inline constexpr T logb(T x)
	{
		static_assert(FLT_RADIX == 2);
		return log2<T>(x);
	}

	template<floating_point T>
	inline constexpr T exp2(T x)
	{
		if (abs(x) <= (T)1.0)
		{
			asm("f2xm1" : "+t"(x));
			return x + (T)1.0;
		}

		asm(
			"fld1;"
			"fld %%st(1);"
			"fprem;"
			"f2xm1;"
			"faddp;"
			"fscale;"
			"fstp %%st(1);"
			: "+t"(x)
		);
		return x;
	}

	template<floating_point T>
	inline constexpr T exp(T x)
	{
		return exp2<T>(x * numbers::log2e_v<T>);
	}

	template<floating_point T>
	inline constexpr T pow(T x, T y)
	{
		asm(
			"fyl2x;"
			"fld1;"
			"fld %%st(1);"
			"fprem;"
			"f2xm1;"
			"faddp;"
			"fscale;"
			: "+t"(x), "+u"(y)
		);

		return x;
	}

	template<floating_point T>
	inline constexpr T scalbn(T x, int n)
	{
		asm("fscale" : "+t"(x) : "u"(static_cast<T>(n)));
		return x;
	}

	template<floating_point T>
	inline constexpr T ldexp(T x, int y)
	{
		const bool exp_sign = y < 0;
		if (exp_sign)
			y = -y;

		T exp = (T)1.0;
		T mult = (T)2.0;
		while (y)
		{
			if (y & 1)
				exp *= mult;
			mult *= mult;
			y >>= 1;
		}

		if (exp_sign)
			exp = (T)1.0 / exp;

		return x * exp;
	}

	template<floating_point T>
	inline constexpr T sqrt(T x)
	{
		asm("fsqrt" : "+t"(x));
		return x;
	}

	template<floating_point T>
	inline constexpr T cbrt(T value)
	{
		if (value == 0.0)
			return 0.0;
		return pow<T>(value, 1.0 / 3.0);
	}

	template<floating_point T>
	inline constexpr T sin(T x)
	{
		asm("fsin" : "+t"(x));
		return x;
	}

	template<floating_point T>
	inline constexpr T cos(T x)
	{
		asm("fcos" : "+t"(x));
		return x;
	}

	template<floating_point T>
	inline constexpr void sincos(T x, T& sin, T& cos)
	{
		asm("fsincos" : "=t"(cos), "=u"(sin) : "0"(x));
	}

	template<floating_point T>
	inline constexpr T tan(T x)
	{
		T one, ret;
		asm(
			"fptan"
			: "=t"(one), "=u"(ret)
			: "0"(x)
		);
		return ret;
	}

	template<floating_point T>
	inline constexpr T atan2(T y, T x)
	{
		asm(
			"fpatan"
			: "+t"(x)
			: "u"(y)
			: "st(1)"
		);
		return x;
	}

	template<floating_point T>
	inline constexpr T atan(T x)
	{
		return atan2<T>(x, 1.0);
	}

	template<floating_point T>
	inline constexpr T asin(T x)
	{
		if (x == (T)0.0)
			return (T)0.0;
		if (x == (T)1.0)
			return  numbers::pi_v<T> / (T)2.0;
		if (x == (T)-1.0)
			return -numbers::pi_v<T> / (T)2.0;
		return (T)2.0 * atan<T>(x / (T(1.0) + sqrt<T>((T)1.0 - x * x)));
	}

	template<floating_point T>
	inline constexpr T acos(T x)
	{
		if (x == (T)0.0)
			return numbers::pi_v<T> / (T)2.0;
		if (x == (T)1.0)
			return (T)0.0;
		if (x == (T)-1.0)
			return numbers::pi_v<T>;
		return (T)2.0 * atan<T>(sqrt<T>((T)1.0 - x * x) / ((T)1.0 + x));
	}

	template<floating_point T>
	inline constexpr T sinh(T x)
	{
		return (exp<T>(x) - exp<T>(-x)) / (T)2.0;
	}

	template<floating_point T>
	inline constexpr T cosh(T x)
	{
		return (exp<T>(x) + exp<T>(-x)) / (T)2.0;
	}

	template<floating_point T>
	inline constexpr T tanh(T x)
	{
		const T exp_px = exp<T>(x);
		const T exp_nx = exp<T>(-x);
		return (exp_px - exp_nx) / (exp_px + exp_nx);
	}

	template<floating_point T>
	inline constexpr T asinh(T x)
	{
		return log<T>(x + sqrt<T>(x * x + (T)1.0));
	}

	template<floating_point T>
	inline constexpr T acosh(T x)
	{
		return log<T>(x + sqrt<T>(x * x - (T)1.0));
	}

	template<floating_point T>
	inline constexpr T atanh(T x)
	{
		return (T)0.5 * log<T>(((T)1.0 + x) / ((T)1.0 - x));
	}

	template<floating_point T>
	inline constexpr T hypot(T x, T y)
	{
		return sqrt<T>(x * x + y * y);
	}

}
