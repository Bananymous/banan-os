#pragma once

#include <BAN/Traits.h>

#include <stddef.h>

namespace BAN
{

	template<typename T>
	constexpr remove_reference_t<T>&& move(T&& arg)
	{
		return static_cast<remove_reference_t<T>&&>(arg);
	}

	template<typename T>
	constexpr T&& forward(remove_reference_t<T>& arg)
	{
		return static_cast<T&&>(arg);
	}

	template<typename T>
	constexpr T&& forward(remove_reference_t<T>&& arg)
	{
		static_assert(!is_lvalue_reference_v<T>);
		return static_cast<T&&>(arg);
	}

}

inline void* operator new(size_t, void* addr)	{ return addr; }
inline void* operator new[](size_t, void* addr)	{ return addr; }