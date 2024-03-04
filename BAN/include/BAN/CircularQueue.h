#pragma once

#include <BAN/Assert.h>
#include <BAN/Move.h>
#include <BAN/PlacementNew.h>

#include <stdint.h>
#include <stddef.h>

namespace BAN
{

	template<typename T, size_t S>
	class CircularQueue
	{
	public:
		using size_type = size_t;
		using value_type = T;

	public:
		CircularQueue() = default;
		~CircularQueue();

		void push(const T&);
		void push(T&&);
		template<typename... Args>
		void emplace(Args&&... args);

		void pop();

		const T& front() const;
		T& front();

		size_type size() const { return m_size; }
		bool empty() const { return size() == 0; }
		bool full() const { return size() == capacity(); }

		static constexpr size_type capacity() { return S; }

	private:
		T* element_at(size_type);
		const T* element_at(size_type) const;

	private:
		alignas(T) uint8_t m_storage[sizeof(T) * capacity()];
		size_type m_first { 0 };
		size_type m_size { 0 };
	};

	template<typename T, size_t S>
	CircularQueue<T, S>::~CircularQueue()
	{
		for (size_type i = 0; i < m_size; i++)
			element_at((m_first + i) % capacity())->~T();
	}

	template<typename T, size_t S>
	void CircularQueue<T, S>::push(const T& value)
	{
		emplace(BAN::move(T(value)));
	}

	template<typename T, size_t S>
	void CircularQueue<T, S>::push(T&& value)
	{
		emplace(BAN::move(value));
	}

	template<typename T, size_t S>
	template<typename... Args>
	void CircularQueue<T, S>::emplace(Args&&... args)
	{
		ASSERT(!full());
		new (element_at(((m_first + m_size) % capacity()))) T(BAN::forward<Args>(args)...);
		m_size++;
	}

	template<typename T, size_t S>
	void CircularQueue<T, S>::pop()
	{
		ASSERT(!empty());
		element_at(m_first)->~T();
		m_first = (m_first + 1) % capacity();
		m_size--;
	}

	template<typename T, size_t S>
	const T& CircularQueue<T, S>::front() const
	{
		ASSERT(!empty());
		return *element_at(m_first);
	}

	template<typename T, size_t S>
	T& CircularQueue<T, S>::front()
	{
		ASSERT(!empty());
		return *element_at(m_first);
	}

	template<typename T, size_t S>
	const T* CircularQueue<T, S>::element_at(size_type index) const
	{
		ASSERT(index < capacity());
		return (const T*)(m_storage + index * sizeof(T));
	}

	template<typename T, size_t S>
	T* CircularQueue<T, S>::element_at(size_type index)
	{
		ASSERT(index < capacity());
		return (T*)(m_storage + index * sizeof(T));
	}

}
