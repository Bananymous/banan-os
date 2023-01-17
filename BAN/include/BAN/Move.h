#pragma once

namespace BAN
{

	template<typename T>
	struct RemoveReference { using type = T; };
	template<typename T>
	struct RemoveReference<T&> { using type =  T; };
	template<typename T>
	struct RemoveReference<T&&> { using type = T; };

	template<typename T>
	struct IsLValueReference { static constexpr bool value = false; };
	template<typename T>
	struct IsLValueReference<T&> { static constexpr bool value = true; };


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