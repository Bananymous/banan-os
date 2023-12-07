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

	namespace detail
	{

		template<typename It, typename Comp>
		It sort_quick_partition(It begin, It end, Comp comp)
		{
			It pivot = end; --pivot;

			It it1 = begin;
			for (It it2 = begin; it2 != pivot; ++it2)
			{
				if (comp(*it2, *pivot))
				{
					swap(*it1, *it2);
					++it1;
				}
			}

			swap(*it1, *pivot);

			return it1;
		}

	}

	template<typename It, typename Comp = less<typename It::value_type>>
	void sort_quick(It begin, It end, Comp comp = {})
	{
		{
			It it = begin;
			if (it == end || ++it == end)
				return;
		}

		It mid = detail::sort_quick_partition(begin, end, comp);
		sort_quick(begin, mid, comp);
		sort_quick(++mid, end, comp);
	}

}
