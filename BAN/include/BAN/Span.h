#pragma once

#include <BAN/Assert.h>
#include <BAN/Iterators.h>

#include <stddef.h>

namespace BAN
{

	template<typename T>
	class Span
	{
	public:
		using value_type = T;
		using size_type = size_t;
		using iterator = IteratorSimple<T, Span>;
		using const_iterator = ConstIteratorSimple<T, Span>;

	public:
		Span() = default;
		Span(T*, size_type);
		Span(Span<T>&);
		template<typename S>
		requires(is_same_v<T, const S>)
		Span(const Span<S>&);

		iterator begin() { return iterator(m_data); }
		iterator end() { return iterator(m_data + m_size); }
		const_iterator begin() const { return const_iterator(m_data); }
		const_iterator end() const { return const_iterator(m_data + m_size); }

		T& operator[](size_type);
		const T& operator[](size_type) const;

		T* data();
		const T* data() const;

		bool empty() const;
		size_type size() const;

		void clear();

		Span slice(size_type, size_type = ~size_type(0));

		Span<const T> as_const() const { return Span<const T>(m_data, m_size); }

	private:
		T* m_data = nullptr;
		size_type m_size = 0;
	};

	template<typename T>
	Span<T>::Span(T* data, size_type size)
		: m_data(data)
		, m_size(size)
	{
	}

	template<typename T>
	Span<T>::Span(Span& other)
		: m_data(other.data())
		, m_size(other.size())
	{
	}

	template<typename T>
	template<typename S>
	requires(is_same_v<T, const S>)
	Span<T>::Span(const Span<S>& other)
		: m_data(other.data())
		, m_size(other.size())
	{
	}

	template<typename T>
	T& Span<T>::operator[](size_type index)
	{
		ASSERT(m_data);
		ASSERT(index < m_size);
		return m_data[index];
	}

	template<typename T>
	const T& Span<T>::operator[](size_type index) const
	{
		ASSERT(m_data);
		ASSERT(index < m_size);
		return m_data[index];
	}

	template<typename T>
	T* Span<T>::data()
	{
		return m_data;
	}

	template<typename T>
	const T* Span<T>::data() const
	{
		return m_data;
	}

	template<typename T>
	bool Span<T>::empty() const
	{
		return m_size == 0;
	}

	template<typename T>
	typename Span<T>::size_type Span<T>::size() const
	{
		return m_size;
	}

	template<typename T>
	void Span<T>::clear()
	{
		m_data = nullptr;
		m_size = 0;
	}

	template<typename T>
	Span<T> Span<T>::slice(size_type start, size_type length)
	{
		ASSERT(m_data);
		ASSERT(start <= m_size);
		if (length == ~size_type(0))
			length = m_size - start;
		ASSERT(m_size - start >= length);
		return Span(m_data + start, length);
	}

}
