#pragma once

#include <BAN/Errors.h>
#include <kernel/kmalloc.h>

#if defined(__is_bank)
	#include <kernel/kmalloc.h>
#else
	#include <stdlib.h>
#endif

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>

namespace BAN
{

	template<typename T>
	class Queue
	{
	private:
	#if defined(__is_bank)
		using allocator = kmalloc;
		using deallocator = kfree;
	#else
		using allocator = malloc;
		using deallocator = free;
	#endif


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
		deallocator(m_data);
	}

	template<typename T>
	ErrorOr<void> Queue<T>::Push(const T& value)
	{
		VerifyCapacity(m_size + 1);
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

		size_type new_cap = MAX(m_capacity * 1.5f, m_capacity + 1) * sizeof(T);
		void* new_data = allocator(new_cap);
		if (new_data == nullptr)
			return Error { .message = "Queue: out of memory", .error_code = ErrorCode::OutOfMemory };

		memcpy(new_data, m_data, m_size * sizeof(T));
		deallocator(m_data);

		m_data = (T*)new_data;
		m_capacity = new_cap;
	}

}