#pragma once

#include <BAN/Math.h>
#include <BAN/Swap.h>
#include <BAN/Traits.h>
#include <BAN/Vector.h>

namespace BAN::sort
{

	template<typename It, typename Comp = less<typename It::value_type>>
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

	template<typename It, typename Comp = less<typename It::value_type>>
	void quick_sort(It begin, It end, Comp comp = {})
	{
		if (distance(begin, end) <= 1)
			return;
		It mid = detail::partition(begin, end, comp);
		quick_sort(begin, mid, comp);
		quick_sort(++mid, end, comp);
	}

	template<typename It, typename Comp = less<typename It::value_type>>
	void insertion_sort(It begin, It end, Comp comp = {})
	{
		if (distance(begin, end) <= 1)
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

	namespace detail
	{

		template<typename It, typename Comp>
		void push_heap(It begin, size_t hole_index, size_t top_index, typename It::value_type value, Comp comp)
		{
			size_t parent = (hole_index - 1) / 2;
			while (hole_index > top_index && comp(*next(begin, parent), value))
			{
				*next(begin, hole_index) = move(*next(begin, parent));
				hole_index = parent;
				parent = (hole_index - 1) / 2;
			}
			*next(begin, hole_index) = move(value);
		}

		template<typename It, typename Comp>
		void adjust_heap(It begin, size_t hole_index, size_t len, typename It::value_type value, Comp comp)
		{
			const size_t top_index = hole_index;
			size_t child = hole_index;
			while (child < (len - 1) / 2)
			{
				child = 2 * (child + 1);
				if (comp(*next(begin, child), *next(begin, child - 1)))
					child--;
				*next(begin, hole_index) = move(*next(begin, child));
				hole_index = child;
			}
			if (len % 2 == 0 && child == (len - 2) / 2)
			{
				child = 2 * (child + 1);
				*next(begin, hole_index) = move(*next(begin, child - 1));
				hole_index = child - 1;
			}
			push_heap(begin, hole_index, top_index, move(value), comp);
		}

	}

	template<typename It, typename Comp = less<typename It::value_type>>
	void make_heap(It begin, It end, Comp comp = {})
	{
		const size_t len = distance(begin, end);
		if (len <= 1)
			return;

		size_t parent = (len - 2) / 2;
		while (true)
		{
			detail::adjust_heap(begin, parent, len, move(*next(begin, parent)), comp);

			if (parent == 0)
				break;

			parent--;
		}
	}

	template<typename It, typename Comp = less<typename It::value_type>>
	void sort_heap(It begin, It end, Comp comp = {})
	{
		const size_t len = distance(begin, end);
		if (len <= 1)
			return;

		size_t last = len;
		while (last > 1)
		{
			last--;
			typename It::value_type x = move(*next(begin, last));
			*next(begin, last) = move(*begin);
			detail::adjust_heap(begin, 0, last, move(x), comp);
		}
	}

	template<typename It, typename Comp = less<typename It::value_type>>
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

	template<typename It, typename Comp = less<typename It::value_type>>
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
	requires is_unsigned_v<typename It::value_type> && (radix > 0 && (radix & (radix - 1)) == 0)
	BAN::ErrorOr<void> radix_sort(It begin, It end)
	{
		using value_type = typename It::value_type;

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

	template<typename It, typename Comp = less<typename It::value_type>>
	void sort(It begin, It end, Comp comp = {})
	{
		return intro_sort(begin, end, comp);
	}

}
