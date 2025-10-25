#pragma once

#include "BAN/Errors.h"
#include <BAN/Vector.h>
#include <BAN/Heap.h>

namespace BAN
{

	template<typename T, typename Comp = less<T>>
	class PriorityQueue
	{
	public:
		PriorityQueue() = default;
		PriorityQueue(Comp comp)
			: m_comp(comp)
		{ }

		ErrorOr<void> push(const T& value)
		{
			TRY(m_data.push_back(value));
			push_heap(m_data.begin(), m_data.end());
			return {};
		}

		ErrorOr<void> push(T&& value)
		{
			TRY(m_data.push_back(move(value)));
			push_heap(m_data.begin(), m_data.end());
			return {};
		}

		template<typename... Args>
		ErrorOr<void> emplace(Args&&... args) requires is_constructible_v<T, Args...>
		{
			TRY(m_data.emplace_back(forward<Args>(args)...));
			push_heap(m_data.begin(), m_data.end());
			return {};
		}

		void pop()
		{
			pop_heap(m_data.begin(), m_data.end());
			m_data.pop_back();
		}

		BAN::ErrorOr<void> reserve(Vector<T>::size_type size)
		{
			return m_data.reserve(size);
		}

		T& top() { return m_data.front(); }
		const T& top() const { return m_data.front(); }

		bool empty() const { return m_data.empty(); }
		Vector<T>::size_type size() const { return m_data.size(); }
		Vector<T>::size_type capacity() const { return m_data.capacity(); }

	private:
		Comp m_comp;
		Vector<T> m_data;
	};

}
