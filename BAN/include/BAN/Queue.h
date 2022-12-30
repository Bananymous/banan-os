#pragma once

#include <BAN/Errors.h>
#include <BAN/Math.h>
#include <BAN/Memory.h>

#include <assert.h>
#include <stdint.h>
#include <string.h>

namespace BAN
{

	template<typename T>
	class Queue
	{
	public:
		using size_type = uint32_t;
		using value_type = T;

	public:
		Queue() = default;
		~Queue();

		ErrorOr<void> Push(const T& value);
		void Pop();

		bool Empty() const;
		size_type Size() const;

		const T& Front() const;
		T& Front();

	private:
		ErrorOr<void> VerifyCapacity(size_type size);

	private:
		T*			m_data		= nullptr;
		size_type	m_capacity	= 0;
		size_type	m_size		= 0;
	};

	template<typename T>
	Queue<T>::~Queue()
	{
		for (size_type i = 0; i < m_size; i++)
			m_data[i].~T();
		BAN::deallocator(m_data);
	}

	template<typename T>
	ErrorOr<void> Queue<T>::Push(const T& value)
	{
		TRY(VerifyCapacity(m_size + 1));
		m_data[m_size++] = value;
		return {};
	}

	template<typename T>
	void Queue<T>::Pop()
	{
		assert(m_size > 0);
		m_data->~T();
		memmove(m_data, m_data + 1, sizeof(T) * (--m_size));
	}

	template<typename T>
	bool Queue<T>::Empty() const
	{
		return m_size == 0;
	}

	template<typename T>
	typename Queue<T>::size_type Queue<T>::Size() const
	{
		return m_size;
	}

	template<typename T>
	const T& Queue<T>::Front() const
	{
		assert(m_size > 0);
		return *m_data;
	}

	template<typename T>
	T& Queue<T>::Front()
	{
		assert(m_size > 0);
		return *m_data;
	}

	template<typename T>
	ErrorOr<void> Queue<T>::VerifyCapacity(size_type size)
	{
		if (m_capacity > size)
			return {};

		size_type new_cap = BAN::max<size_type>(m_capacity * 1.5f, m_capacity + 1);
		void* new_data = BAN::allocator(new_cap * sizeof(T));
		if (new_data == nullptr)
			return Error::FromString("Queue: Could not allocate memory");

		memcpy(new_data, m_data, m_size * sizeof(T));
		BAN::deallocator(m_data);

		m_data = (T*)new_data;
		m_capacity = new_cap;

		return {};
	}

}