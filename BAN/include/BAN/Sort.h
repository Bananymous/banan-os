#pragma once

#include <BAN/Swap.h>
#include <BAN/Traits.h>

namespace BAN
{

	template<typename It, typename Comp = less<typename It::value_type>>
	void sort_exchange(It begin, It end, Comp comp = {})
	{
		for (It lhs = begin; lhs != end; ++lhs)
			for (It rhs = lhs; ++rhs != end;)
				if (!comp(*lhs, *rhs))
					swap(*lhs, *rhs);
	}



}
