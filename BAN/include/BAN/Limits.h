#pragma once

#include <BAN/Traits.h>

#include <stdint.h>

namespace BAN
{

	template<typename T>
	class numeric_limits
	{
	public:
		numeric_limits() = delete;

		static inline constexpr T max()
		{
			if constexpr(is_same_v<T, char>)
				return __SCHAR_MAX__;
			if constexpr(is_same_v<T, signed char>)
				return __SCHAR_MAX__;
			if constexpr(is_same_v<T, unsigned char>)
				return (T)__SCHAR_MAX__ * 2 + 1;

			if constexpr(is_same_v<T, short>)
				return __SHRT_MAX__;
			if constexpr(is_same_v<T, int>)
				return __INT_MAX__;
			if constexpr(is_same_v<T, long>)
				return __LONG_MAX__;
			if constexpr(is_same_v<T, long long>)
				return __LONG_LONG_MAX__;

			if constexpr(is_same_v<T, unsigned short>)
				return (T)__SHRT_MAX__ * 2 + 1;
			if constexpr(is_same_v<T, unsigned int>)
				return (T)__INT_MAX__ * 2 + 1;
			if constexpr(is_same_v<T, unsigned long>)
				return (T)__LONG_MAX__ * 2 + 1;
			if constexpr(is_same_v<T, unsigned long long>)
				return (T)__LONG_LONG_MAX__ * 2 + 1;

			if constexpr(is_same_v<T, float>)
				return __FLT_MAX__;
			if constexpr(is_same_v<T, double>)
				return __DBL_MAX__;
			if constexpr(is_same_v<T, long double>)
				return __LDBL_MAX__;
		}

		static inline constexpr T min()
		{
			if constexpr(is_signed_v<T> && is_integral_v<T>)
				return -max() - 1;

			if constexpr(is_unsigned_v<T> && is_integral_v<T>)
				return 0;

			if constexpr(is_same_v<T, float>)
				return __FLT_MIN__;
			if constexpr(is_same_v<T, double>)
				return __DBL_MIN__;
			if constexpr(is_same_v<T, long double>)
				return __LDBL_MIN__;
		}

		static inline constexpr bool has_infinity()
		{
			if constexpr(is_same_v<T, float>)
				return __FLT_HAS_INFINITY__;
			if constexpr(is_same_v<T, double>)
				return __DBL_HAS_INFINITY__;
			if constexpr(is_same_v<T, long double>)
				return __LDBL_HAS_INFINITY__;
			return false;
		}

		static inline constexpr T infinity() requires(has_infinity())
		{
			if constexpr(is_same_v<T, float>)
				return __builtin_inff();
			if constexpr(is_same_v<T, double>)
				return __builtin_inf();
			if constexpr(is_same_v<T, long double>)
				return __builtin_infl();
		}

		static inline constexpr bool has_quiet_NaN()
		{
			if constexpr(is_same_v<T, float>)
				return __FLT_HAS_QUIET_NAN__;
			if constexpr(is_same_v<T, double>)
				return __DBL_HAS_QUIET_NAN__;
			if constexpr(is_same_v<T, long double>)
				return __LDBL_HAS_QUIET_NAN__;
			return false;
		}

		static inline constexpr T quiet_NaN() requires(has_quiet_NaN())
		{
			if constexpr(is_same_v<T, float>)
				return __builtin_nanf("");
			if constexpr(is_same_v<T, double>)
				return __builtin_nan("");
			if constexpr(is_same_v<T, long double>)
				return __builtin_nanl("");
		}

		static inline constexpr int max_exponent2()
		{
			static_assert(__FLT_RADIX__ == 2);
			if constexpr(is_same_v<T, float>)
				return __FLT_MAX_EXP__;
			if constexpr(is_same_v<T, double>)
				return __DBL_MAX_EXP__;
			if constexpr(is_same_v<T, long double>)
				return __LDBL_MAX_EXP__;
			return 0;
		}

		static inline constexpr int max_exponent10()
		{
			if constexpr(is_same_v<T, float>)
				return __FLT_MAX_10_EXP__;
			if constexpr(is_same_v<T, double>)
				return __DBL_MAX_10_EXP__;
			if constexpr(is_same_v<T, long double>)
				return __LDBL_MAX_10_EXP__;
			return 0;
		}

		static inline constexpr int min_exponent2()
		{
			static_assert(__FLT_RADIX__ == 2);
			if constexpr(is_same_v<T, float>)
				return __FLT_MIN_EXP__;
			if constexpr(is_same_v<T, double>)
				return __DBL_MIN_EXP__;
			if constexpr(is_same_v<T, long double>)
				return __LDBL_MIN_EXP__;
			return 0;
		}

		static inline constexpr int min_exponent10()
		{
			if constexpr(is_same_v<T, float>)
				return __FLT_MIN_10_EXP__;
			if constexpr(is_same_v<T, double>)
				return __DBL_MIN_10_EXP__;
			if constexpr(is_same_v<T, long double>)
				return __LDBL_MIN_10_EXP__;
			return 0;
		}
	};

}
