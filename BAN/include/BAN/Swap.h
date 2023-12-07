#pragma once

#include <BAN/Move.h>

namespace BAN
{

	template<typename T>
	void swap(T& lhs, T& rhs)
	{
		T tmp = move(lhs);
		lhs = move(rhs);
		rhs = move(tmp);
	}

}
