#pragma once

#include <BAN/Errors.h>
#include <BAN/Iterators.h>

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
		Array();
		Array(const T&);

		iterator begin() { return iterator(m_data); }
		iterator end() { return iterator(m_data + size()); }
		const_iterator begin() const { return const_iterator(m_data); }
		const_iterator end() const { return const_iterator(m_data + size()); }

		const T& operator[](size_type) const;
		T& operator[](size_type);

		const T& back() const;
		T& back();
		const T& front() const;
		T& front();

		constexpr size_type size() const;

	private:
		T m_data[S];
	};

	template<typename T, size_t S>
	Array<T, S>::Array()
	{
		for (size_type i = 0; i < S; i++)
			m_data[i] = T();
	}

	template<typename T, size_t S>
	Array<T, S>::Array(const T& value)
	{
		for (size_type i = 0; i < S; i++)
			m_data[i] = value;
	}

	template<typename T, size_t S>
	const T& Array<T, S>::operator[](size_type index) const
	{
		ASSERT(index < S);
		return m_data[index];
	}

	template<typename T, size_t S>
	T& Array<T, S>::operator[](size_type index)
	{
		ASSERT(index < S);
		return m_data[index];
	}

	template<typename T, size_t S>
	const T& Array<T, S>::back() const
	{
		ASSERT(S != 0);
		return m_data[S - 1];
	}

	template<typename T, size_t S>
	T& Array<T, S>::back()
	{
		ASSERT(S != 0);
		return m_data[S - 1];
	}

	template<typename T, size_t S>
	const T& Array<T, S>::front() const
	{
		ASSERT(S != 0);
		return m_data[0];
	}

	template<typename T, size_t S>
	T& Array<T, S>::front()
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