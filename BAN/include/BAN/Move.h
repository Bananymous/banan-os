#pragma once

#include <BAN/Traits.h>

namespace BAN
{

	template<typename T>
	constexpr typename RemoveReference<T>::type&& Move(T&& arg)
	{
		return static_cast<typename RemoveReference<T>::type&&>(arg);
	}

	template<typename T>
	constexpr T&& Forward(typename RemoveReference<T>::type& arg)
	{
		return static_cast<T&&>(arg);
	}

	template<typename T>
	constexpr T&& Forward(typename RemoveReference<T>::type&& arg)
	{
		static_assert(!IsLValueReference<T>::value);
		return static_cast<T&&>(arg);
	}

}