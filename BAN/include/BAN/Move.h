#pragma once

namespace BAN
{

	template <class T>
	struct RemoveReference { typedef T type; };

	template <class T>
	struct RemoveReference<T&> { typedef T type; };

	template <class T>
	struct RemoveReference<T&&> { typedef T type; };

	template<class T>
	typename RemoveReference<T>::type&&
	Move( T&& Arg ) { return (typename RemoveReference<T>::type&&)Arg; }

}