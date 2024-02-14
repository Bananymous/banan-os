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

		static constexpr T max() requires(is_signed_v<T> && is_integral_v<T>)
		{
			if constexpr(sizeof(T) == sizeof(int8_t))
				return __INT8_MAX__;
			if constexpr(sizeof(T) == sizeof(int16_t))
				return __INT16_MAX__;
			if constexpr(sizeof(T) == sizeof(int32_t))
				return __INT32_MAX__;
			if constexpr(sizeof(T) == sizeof(int64_t))
				return __INT64_MAX__;
		}

		static constexpr T max() requires(is_unsigned_v<T> && is_integral_v<T>)
		{
			if constexpr(sizeof(T) == sizeof(uint8_t))
				return __UINT8_MAX__;
			if constexpr(sizeof(T) == sizeof(uint16_t))
				return __UINT16_MAX__;
			if constexpr(sizeof(T) == sizeof(uint32_t))
				return __UINT32_MAX__;
			if constexpr(sizeof(T) == sizeof(uint64_t))
				return __UINT64_MAX__;
		}

		static constexpr T max() requires(is_floating_point_v<T>)
		{
			if constexpr(sizeof(T) == sizeof(float))
				return __FLT_MAX__;
			if constexpr(sizeof(T) == sizeof(double))
				return __DBL_MAX__;
			if constexpr(sizeof(T) == sizeof(long double))
				return __LDBL_MAX__;
		}

		static constexpr T infinity() requires(is_floating_point_v<T>)
		{
			if constexpr(sizeof(T) == sizeof(float))
				return __FLT_MAX__;
			if constexpr(sizeof(T) == sizeof(double))
				return __DBL_MAX__;
			if constexpr(sizeof(T) == sizeof(long double))
				return __LDBL_MAX__;
		}

		static constexpr T nan() requires(is_floating_point_v<T>)
		{
			if constexpr(sizeof(T) == sizeof(float))
				return __builtin_nanf("");
			if constexpr(sizeof(T) == sizeof(double))
				return __builtin_nan("");
			if constexpr(sizeof(T) == sizeof(long double))
				return __builtin_nanl("");
		}
	};

}
