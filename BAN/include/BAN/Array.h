#pragma once

#include <BAN/Iterators.h>
#include <BAN/Span.h>

#include <stddef.h>

namespace BAN
{

	template<typename T, size_t S>
	class Array
	{
	public:
		using size_type = decltype(S);
		using value_type = T;
		using iterator = IteratorSimple<T, Array>;
		using const_iterator = ConstIteratorSimple<T, Array>;

	public:
		constexpr Array() = default;
		constexpr Array(const T&);

		iterator begin() { return iterator(m_data); }
		iterator end() { return iterator(m_data + size()); }
		const_iterator begin() const { return const_iterator(m_data); }
		const_iterator end() const { return const_iterator(m_data + size()); }

		constexpr const T& operator[](size_type) const;
		constexpr T& operator[](size_type);

		constexpr const T& back() const;
		constexpr T& back();
		constexpr const T& front() const;
		constexpr T& front();

		Span<T> span() { return Span(m_data, size()); }
		const Span<T> span() const { return Span(m_data, size()); }

		constexpr size_type size() const;

		constexpr const T* data() const { return m_data; }
		constexpr T* data() { return m_data; }

	private:
		T m_data[S] {};
	};

	template<typename T, size_t S>
	constexpr Array<T, S>::Array(const T& value)
	{
		for (size_type i = 0; i < S; i++)
			m_data[i] = value;
	}

	template<typename T, size_t S>
	constexpr const T& Array<T, S>::operator[](size_type index) const
	{
		ASSERT(index < S);
		return m_data[index];
	}

	template<typename T, size_t S>
	constexpr T& Array<T, S>::operator[](size_type index)
	{
		ASSERT(index < S);
		return m_data[index];
	}

	template<typename T, size_t S>
	constexpr const T& Array<T, S>::back() const
	{
		ASSERT(S != 0);
		return m_data[S - 1];
	}

	template<typename T, size_t S>
	constexpr T& Array<T, S>::back()
	{
		ASSERT(S != 0);
		return m_data[S - 1];
	}

	template<typename T, size_t S>
	constexpr const T& Array<T, S>::front() const
	{
		ASSERT(S != 0);
		return m_data[0];
	}

	template<typename T, size_t S>
	constexpr T& Array<T, S>::front()
	{
		ASSERT(S != 0);
		return m_data[0];
	}

	template<typename T, size_t S>
	constexpr typename Array<T, S>::size_type Array<T, S>::size() const
	{
		return S;
	}

}
