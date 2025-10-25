#pragma once

#include <BAN/Heap.h>
#include <BAN/Math.h>
#include <BAN/Swap.h>
#include <BAN/Traits.h>
#include <BAN/Vector.h>

namespace BAN::sort
{

	template<typename It, typename Comp = less<it_value_type_t<It>>>
	void exchange_sort(It begin, It end, Comp comp = {})
	{
		for (It lhs = begin; lhs != end; ++lhs)
			for (It rhs = next(lhs, 1); rhs != end; ++rhs)
				if (!comp(*lhs, *rhs))
					swap(*lhs, *rhs);
	}

	namespace detail
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

	template<typename It, typename Comp = less<it_value_type_t<It>>>
	void quick_sort(It begin, It end, Comp comp = {})
	{
		if (distance(begin, end) <= 1)
			return;
		It mid = detail::partition(begin, end, comp);
		quick_sort(begin, mid, comp);
		quick_sort(++mid, end, comp);
	}

	template<typename It, typename Comp = less<it_value_type_t<It>>>
	void insertion_sort(It begin, It end, Comp comp = {})
	{
		if (distance(begin, end) <= 1)
			return;
		for (It it1 = next(begin, 1); it1 != end; ++it1)
		{
			auto x = move(*it1);
			It it2 = it1;
			for (; it2 != begin && comp(x, *prev(it2, 1)); --it2)
				*it2 = move(*prev(it2, 1));
			*it2 = move(x);
		}
	}

	template<typename It, typename Comp = less<it_value_type_t<It>>>
	void heap_sort(It begin, It end, Comp comp = {})
	{
		make_heap(begin, end, comp);
		sort_heap(begin, end, comp);
	}

	namespace detail
	{

		template<typename It, typename Comp>
		void intro_sort_impl(It begin, It end, size_t max_depth, Comp comp)
		{
			if (distance(begin, end) <= 16)
				return insertion_sort(begin, end, comp);
			if (max_depth == 0)
				return heap_sort(begin, end, comp);
			It mid = detail::partition(begin, end, comp);
			intro_sort_impl(begin, mid, max_depth - 1, comp);
			intro_sort_impl(++mid, end, max_depth - 1, comp);
		}

	}

	template<typename It, typename Comp = less<it_value_type_t<It>>>
	void intro_sort(It begin, It end, Comp comp = {})
	{
		const size_t len = distance(begin, end);
		if (len <= 1)
			return;
		detail::intro_sort_impl(begin, end, 2 * Math::ilog2(len), comp);
	}

	namespace detail
	{

		template<unsigned_integral T>
		consteval T lsb_index(T value)
		{
			for (T result = 0;; result++)
				if (value & (1 << result))
					return result;
		}

	}

	template<typename It, size_t radix = 256>
	requires is_unsigned_v<it_value_type_t<It>> && (radix > 0 && (radix & (radix - 1)) == 0)
	BAN::ErrorOr<void> radix_sort(It begin, It end)
	{
		using value_type = it_value_type_t<It>;

		const size_t len = distance(begin, end);
		if (len <= 1)
			return {};

		Vector<value_type> temp;
		TRY(temp.resize(len));

		Vector<size_t> counts;
		TRY(counts.resize(radix));

		constexpr size_t mask  = radix - 1;
		constexpr size_t shift = detail::lsb_index(radix);

		for (size_t s = 0; s < sizeof(value_type) * 8; s += shift)
		{
			for (auto& cnt : counts)
				cnt = 0;
			for (It it = begin; it != end; ++it)
				counts[(*it >> s) & mask]++;

			for (size_t i = 0; i < radix - 1; i++)
				counts[i + 1] += counts[i];

			for (It it = end; it != begin;)
			{
				--it;
				temp[--counts[(*it >> s) & mask]] = *it;
			}

			for (size_t j = 0; j < temp.size(); j++)
				*next(begin, j) = temp[j];
		}

		return {};
	}

	template<typename It, typename Comp = less<it_value_type_t<It>>>
	void sort(It begin, It end, Comp comp = {})
	{
		return intro_sort(begin, end, comp);
	}

}
