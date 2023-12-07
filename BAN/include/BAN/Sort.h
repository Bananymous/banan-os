#pragma once

#include <BAN/Swap.h>
#include <BAN/Traits.h>

namespace BAN
{

	template<typename It, typename Comp = less<typename It::value_type>>
	void sort_exchange(It begin, It end, Comp comp = {})
	{
		for (It lhs = begin; lhs != end; ++lhs)
			for (It rhs = next(lhs, 1); rhs != end; ++rhs)
				if (!comp(*lhs, *rhs))
					swap(*lhs, *rhs);
	}

	namespace detail::sort
	{

		template<typename It, typename Comp>
		It partition(It begin, It end, Comp comp)
		{
			It pivot = prev(end, 1);

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
		if (begin == end || next(begin, 1) == end)
			return;
		It mid = detail::sort::partition(begin, end, comp);
		sort_quick(begin, mid, comp);
		sort_quick(++mid, end, comp);
	}

	template<typename It, typename Comp = less<typename It::value_type>>
	void sort_insertion(It begin, It end, Comp comp = {})
	{
		if (begin == end || next(begin, 1) == end)
			return;
		for (It it1 = next(begin, 1); it1 != end; ++it1)
		{
			typename It::value_type x = move(*it1);
			It it2 = it1;
			for (; it2 != begin && comp(x, *prev(it2, 1)); --it2)
				*it2 = move(*prev(it2, 1));
			*it2 = move(x);
		}
	}

	template<typename It, typename Comp = less<typename It::value_type>>
	void sort_heap(It begin, It end, Comp comp = {})
	{
		if (begin == end || next(begin, 1) == end)
			return;

		It start = next(begin, distance(begin, end) / 2);

		while (prev(end, 1) != begin)
		{
			if (start != begin)
				--start;
			else
				swap(*(--end), *begin);

			It root = start;
			while (true)
			{
				size_t left_child = 2 * distance(begin, root) + 1;
				if (left_child >= distance(begin, end))
					break;

				It child = next(begin, left_child);
				if (next(child, 1) != end && comp(*child, *next(child, 1)))
					++child;

				if (!comp(*root, *child))
					break;

				swap(*root, *child);
				root = child;
			}
		}
	}

}
