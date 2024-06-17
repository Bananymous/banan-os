#pragma once

#include <BAN/Span.h>

#include <stdint.h>

namespace BAN
{

	template<bool CONST>
	class ByteSpanGeneral
	{
	public:
		using value_type = maybe_const_t<CONST, uint8_t>;
		using size_type = size_t;

	public:
		ByteSpanGeneral() = default;
		ByteSpanGeneral(value_type* data, size_type size)
			: m_data(data)
			, m_size(size)
		{ }

		ByteSpanGeneral(ByteSpanGeneral& other)
			: m_data(other.data())
			, m_size(other.size())
		{ }
		ByteSpanGeneral(ByteSpanGeneral&& other)
			: m_data(other.data())
			, m_size(other.size())
		{
			other.m_data = nullptr;
			other.m_size = 0;
		}
		template<bool C2>
		ByteSpanGeneral(const ByteSpanGeneral<C2>& other) requires(CONST)
			: m_data(other.data())
			, m_size(other.size())
		{ }
		template<bool C2>
		ByteSpanGeneral(ByteSpanGeneral<C2>&& other) requires(CONST)
			: m_data(other.data())
			, m_size(other.size())
		{
			other.m_data = nullptr;
			other.m_size = 0;
		}
		ByteSpanGeneral(Span<uint8_t> other)
			: m_data(other.data())
			, m_size(other.size())
		{ }
		ByteSpanGeneral(const Span<const uint8_t>& other) requires(CONST)
			: m_data(other.data())
			, m_size(other.size())
		{ }

		ByteSpanGeneral& operator=(ByteSpanGeneral other)
		{
			m_data = other.data();
			m_size = other.size();
			return *this;
		}
		template<bool C2>
		ByteSpanGeneral& operator=(const ByteSpanGeneral<C2>& other) requires(CONST)
		{
			m_data = other.data();
			m_size = other.size();
			return *this;
		}
		ByteSpanGeneral& operator=(Span<uint8_t> other)
		{
			m_data = other.data();
			m_size = other.size();
			return *this;
		}
		ByteSpanGeneral& operator=(const Span<const uint8_t>& other) requires(CONST)
		{
			m_data = other.data();
			m_size = other.size();
			return *this;
		}

		template<typename S>
		requires(CONST || !is_const_v<S>)
		static ByteSpanGeneral from(S& value)
		{
			return ByteSpanGeneral(reinterpret_cast<value_type*>(&value), sizeof(S));
		}

		template<typename S>
		requires(!CONST && !is_const_v<S>)
		S& as()
		{
			ASSERT(m_data);
			ASSERT(m_size >= sizeof(S));
			return *reinterpret_cast<S*>(m_data);
		}

		template<typename S>
		requires(is_const_v<S>)
		S& as() const
		{
			ASSERT(m_data);
			ASSERT(m_size >= sizeof(S));
			return *reinterpret_cast<S*>(m_data);
		}

		template<typename S>
		requires(!CONST && !is_const_v<S>)
		Span<S> as_span()
		{
			ASSERT(m_data);
			return Span<S>(reinterpret_cast<S*>(m_data), m_size / sizeof(S));
		}

		template<typename S>
		const Span<S> as_span() const
		{
			ASSERT(m_data);
			return Span<S>(reinterpret_cast<S*>(m_data), m_size / sizeof(S));
		}

		ByteSpanGeneral slice(size_type offset, size_type length = size_type(-1))
		{
			ASSERT(m_data);
			ASSERT(m_size >= offset);
			if (length == size_type(-1))
				length = m_size - offset;
			ASSERT(m_size >= offset + length);
			return ByteSpanGeneral(m_data + offset, length);
		}

		value_type& operator[](size_type offset)
		{
			ASSERT(offset < m_size);
			return m_data[offset];
		}
		const value_type& operator[](size_type offset) const
		{
			ASSERT(offset < m_size);
			return m_data[offset];
		}

		value_type* data() { return m_data; }
		const value_type* data() const { return m_data; }

		size_type size() const { return m_size; }

	private:
		value_type* m_data { nullptr };
		size_type m_size { 0 };

		friend class ByteSpanGeneral<!CONST>;
	};

	using ByteSpan = ByteSpanGeneral<false>;
	using ConstByteSpan = ByteSpanGeneral<true>;

}
