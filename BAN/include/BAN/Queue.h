#pragma once

#include <BAN/Errors.h>
#include <BAN/Math.h>
#include <BAN/Memory.h>
#include <BAN/Move.h>

namespace BAN
{

	template<typename T>
	class Queue
	{
	public:
		using size_type = size_t;
		using value_type = T;

	public:
		Queue() = default;
		Queue(Queue<T>&&);
		Queue(const Queue<T>&);
		~Queue();

		Queue<T>& operator=(Queue<T>&&);
		Queue<T>& operator=(const Queue<T>&);

		ErrorOr<void> push(T&&);
		ErrorOr<void> push(const T&);
		template<typename... Args>
		ErrorOr<void> emplace(Args&&...);

		ErrorOr<void> reserve(size_type);
		ErrorOr<void> shrink_to_fit();

		void pop();
		void clear();

		bool empty() const;
		size_type size() const;

		const T& front() const;
		T& front();

	private:
		ErrorOr<void> ensure_capacity(size_type size);

	private:
		T*			m_data		= nullptr;
		size_type	m_capacity	= 0;
		size_type	m_size		= 0;
	};

	template<typename T>
	Queue<T>::Queue(Queue<T>&& other)
	{
		m_data = other.m_data;
		m_capacity = other.m_capacity;
		m_size = other.m_size;

		other.m_data = nullptr;
		other.m_capacity = 0;
		other.m_size = 0;
	}

	template<typename T>
	Queue<T>::Queue(const Queue<T>& other)
	{
		MUST(ensure_capacity(other.size()));
		for (size_type i = 0; i < other.size(); i++)
			new (m_data + i) T(other.m_data[i]);
		m_size = other.m_size;
	}

	template<typename T>
	Queue<T>::~Queue()
	{
		clear();
	}

	template<typename T>
	Queue<T>& Queue<T>::operator=(Queue<T>&& other)
	{
		clear();

		m_data = other.m_data;
		m_capacity = other.m_capacity;
		m_size = other.m_size;

		other.m_data = nullptr;
		other.m_capacity = 0;
		other.m_size = 0;

		return *this;
	}

	template<typename T>
	Queue<T>& Queue<T>::operator=(const Queue<T>& other)
	{
		clear();
		MUST(ensure_capacity(other.size()));
		for (size_type i = 0; i < other.size(); i++)
			new (m_data + i) T(other.m_data[i]);
		m_size = other.m_size;
		return *this;
	}

	template<typename T>
	ErrorOr<void> Queue<T>::push(T&& value)
	{
		TRY(ensure_capacity(m_size + 1));
		new (m_data + m_size) T(move(value));
		m_size++;
		return {};
	}

	template<typename T>
	ErrorOr<void> Queue<T>::push(const T& value)
	{
		return push(move(T(value)));
	}

	template<typename T>
	template<typename... Args>
	ErrorOr<void> Queue<T>::emplace(Args&&... args)
	{
		TRY(ensure_capacity(m_size + 1));
		new (m_data + m_size) T(forward<Args>(args)...);
		m_size++;
		return {};
	}

	template<typename T>
	ErrorOr<void> Queue<T>::reserve(size_type size)
	{
		TRY(ensure_capacity(size));
		return {};
	}

	template<typename T>
	ErrorOr<void> Queue<T>::shrink_to_fit()
	{
		size_type temp = m_capacity;
		m_capacity = 0;
		auto error_or = ensure_capacity(m_size);
		if (error_or.is_error())
		{
			m_capacity = temp;
			return error_or;
		}
		return {};
	}

	template<typename T>
	void Queue<T>::pop()
	{
		ASSERT(m_size > 0);
		for (size_type i = 0; i < m_size - 1; i++)
			m_data[i] = move(m_data[i + 1]);
		m_data[m_size - 1].~T();
		m_size--;
	}

	template<typename T>
	void Queue<T>::clear()
	{
		for (size_type i = 0; i < m_size; i++)
			m_data[i].~T();
		BAN::deallocator(m_data);
		m_data = nullptr;
		m_capacity = 0;
		m_size = 0;
	}

	template<typename T>
	bool Queue<T>::empty() const
	{
		return m_size == 0;
	}

	template<typename T>
	typename Queue<T>::size_type Queue<T>::size() const
	{
		return m_size;
	}

	template<typename T>
	const T& Queue<T>::front() const
	{
		ASSERT(m_size > 0);
		return m_data[0];
	}

	template<typename T>
	T& Queue<T>::front()
	{
		ASSERT(m_size > 0);
		return m_data[0];
	}

	template<typename T>
	ErrorOr<void> Queue<T>::ensure_capacity(size_type size)
	{
		if (m_capacity > size)
			return {};
		size_type new_cap = BAN::Math::max<size_type>(size, m_capacity * 2);
		T* new_data = (T*)BAN::allocator(new_cap * sizeof(T));
		if (new_data == nullptr)
			return Error::from_errno(ENOMEM);
		for (size_type i = 0; i < m_size; i++)
		{
			new (new_data + i) T(move(m_data[i]));
			m_data[i].~T();
		}
		BAN::deallocator(m_data);
		m_data = new_data;
		m_capacity = new_cap;
		return {};
	}

}