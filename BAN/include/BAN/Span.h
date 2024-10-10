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
		using iterator = IteratorSimple<value_type, Span>;
		using const_iterator = ConstIteratorSimple<value_type, Span>;

	private:
		template<typename S>
		static inline constexpr bool can_init_from_v = is_same_v<value_type, const S> || is_same_v<value_type, S>;

	public:
		Span() = default;
		Span(value_type* data, size_type size)
			: m_data(data)
			, m_size(size)
		{ }

		template<typename S>
		Span(const Span<S>& other) requires can_init_from_v<S>
			: m_data(other.m_data)
			, m_size(other.m_size)
		{ }
		template<typename S>
		Span(Span<S>&& other) requires can_init_from_v<S>
			: m_data(other.m_data)
			, m_size(other.m_size)
		{
			other.clear();
		}

		template<typename S>
		Span& operator=(const Span<S>& other) requires can_init_from_v<S>
		{
			m_data = other.m_data;
			m_size = other.m_size;
			return *this;
		}
		template<typename S>
		Span& operator=(Span<S>&& other) requires can_init_from_v<S>
		{
			m_data = other.m_data;
			m_size = other.m_size;
			return *this;
		}

		iterator begin() { return iterator(m_data); }
		iterator end() { return iterator(m_data + m_size); }
		const_iterator begin() const { return const_iterator(m_data); }
		const_iterator end() const { return const_iterator(m_data + m_size); }

		value_type& operator[](size_type index) const
		{
			ASSERT(index < m_size);
			return m_data[index];
		}

		value_type* data() const
		{
			ASSERT(m_data);
			return m_data;
		}

		bool empty() const { return m_size == 0; }
		size_type size() const { return m_size; }

		void clear()
		{
			m_data = nullptr;
			m_size = 0;
		}

		Span slice(size_type start, size_type length = ~size_type(0)) const
		{
			ASSERT(m_data);
			ASSERT(start <= m_size);
			if (length == ~size_type(0))
				length = m_size - start;
			ASSERT(m_size - start >= length);
			return Span(m_data + start, length);
		}

		Span<const value_type> as_const() const { return *this; }

	private:
		value_type* m_data = nullptr;
		size_type m_size = 0;

		friend class Span<const value_type>;
	};

}
