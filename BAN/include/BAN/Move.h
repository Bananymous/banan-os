#pragma once

#include <BAN/Traits.h>

namespace BAN
{

	template<typename T>
	constexpr remove_reference_t<T>&& Move(T&& arg)
	{
		return static_cast<remove_reference_t<T>&&>(arg);
	}

	template<typename T>
	constexpr T&& Forward(remove_reference_t<T>& arg)
	{
		return static_cast<T&&>(arg);
	}

	template<typename T>
	constexpr T&& Forward(remove_reference_t<T>&& arg)
	{
		static_assert(!is_lvalue_reference_v<T>);
		return static_cast<T&&>(arg);
	}

}