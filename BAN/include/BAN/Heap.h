#pragma once

#include <BAN/Iteration.h>
#include <BAN/Swap.h>
#include <BAN/Traits.h>
#include <cstddef>

namespace BAN
{

	namespace detail
	{

		template<typename It, typename Comp>
		void heapify_up(It begin, size_t index, Comp comp)
		{
			size_t parent = (index - 1) / 2;
			while (parent < index)
			{
				if (comp(*(begin + index), *(begin + parent)))
					break;
				swap(*(begin + parent), *(begin + index));
				index = parent;
				parent = (index - 1) / 2;
			}
		}

		template<typename It, typename Comp>
		void heapify_down(It begin, size_t index, size_t len, Comp comp)
		{
			for (;;)
			{
				const size_t lchild = 2 * index + 1;
				const size_t rchild = 2 * index + 2;

				size_t child = 0;
				if (lchild < len && !comp(*(begin + lchild), *(begin + index)))
				{
					if (rchild < len && !comp(*(begin + rchild), *(begin + lchild)))
						child = rchild;
					else
						child = lchild;
				}
				else if (rchild < len && !comp(*(begin + rchild), *(begin + index)))
					child = rchild;
				else
					break;

				swap(*(begin + child), *(begin + index));
				index = child;
			}
		}

	}

	template<typename It, typename Comp = less<it_value_type_t<It>>>
	void make_heap(It begin, It end, Comp comp = {})
	{
		const size_t len = distance(begin, end);
		if (len <= 1)
			return;

		size_t index = (len - 2) / 2;
		while (index < len)
			detail::heapify_down(begin, index--, len, comp);
	}

	template<typename It, typename Comp = less<it_value_type_t<It>>>
	void push_heap(It begin, It end, Comp comp = {})
	{
		const size_t len = distance(begin, end);
		detail::heapify_up(begin, len - 1, comp);
	}

	template<typename It, typename Comp = less<it_value_type_t<It>>>
	void pop_heap(It begin, It end, Comp comp = {})
	{
		const size_t len = distance(begin, end);
		swap(*begin, *(begin + len - 1));
		detail::heapify_down(begin, 0, len - 1, comp);
	}

	template<typename It, typename Comp = less<it_value_type_t<It>>>
	void sort_heap(It begin, It end, Comp comp = {})
	{
		while (begin != end)
			pop_heap(begin, end--, comp);
	}

}
