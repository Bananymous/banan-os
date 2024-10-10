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

		template<bool SRC_CONST>
		ByteSpanGeneral(const ByteSpanGeneral<SRC_CONST>& other) requires(CONST || !SRC_CONST)
			: m_data(other.data())
			, m_size(other.size())
		{ }
		template<bool SRC_CONST>
		ByteSpanGeneral(ByteSpanGeneral<SRC_CONST>&& other) requires(CONST || !SRC_CONST)
			: m_data(other.data())
			, m_size(other.size())
		{
			other.clear();
		}

		template<typename T>
		ByteSpanGeneral(const Span<T>& other) requires(is_same_v<T, uint8_t> || (is_same_v<T, const uint8_t> && CONST))
			: m_data(other.data())
			, m_size(other.size())
		{ }
		template<typename T>
		ByteSpanGeneral(Span<T>&& other) requires(is_same_v<T, uint8_t> || (is_same_v<T, const uint8_t> && CONST))
			: m_data(other.data())
			, m_size(other.size())
		{
			other.clear();
		}

		template<bool SRC_CONST>
		ByteSpanGeneral& operator=(const ByteSpanGeneral<SRC_CONST>& other) requires(CONST || !SRC_CONST)
		{
			m_data = other.data();
			m_size = other.size();
			return *this;
		}
		template<bool SRC_CONST>
		ByteSpanGeneral& operator=(ByteSpanGeneral<SRC_CONST>&& other) requires(CONST || !SRC_CONST)
		{
			m_data = other.data();
			m_size = other.size();
			other.clear();
			return *this;
		}

		template<typename S>
		static ByteSpanGeneral from(S& value) requires(CONST || !is_const_v<S>)
		{
			return ByteSpanGeneral(reinterpret_cast<value_type*>(&value), sizeof(S));
		}

		template<typename S>
		S& as() const requires(!CONST || is_const_v<S>)
		{
			ASSERT(m_data);
			ASSERT(m_size >= sizeof(S));
			return *reinterpret_cast<S*>(m_data);
		}

		template<typename S>
		Span<S> as_span() const requires(!CONST || is_const_v<S>)
		{
			ASSERT(m_data);
			return Span<S>(reinterpret_cast<S*>(m_data), m_size / sizeof(S));
		}

		ByteSpanGeneral slice(size_type offset, size_type length = size_type(-1)) const
		{
			ASSERT(m_data);
			ASSERT(m_size >= offset);
			if (length == size_type(-1))
				length = m_size - offset;
			ASSERT(m_size >= offset + length);
			return ByteSpanGeneral(m_data + offset, length);
		}

		value_type& operator[](size_type offset) const
		{
			ASSERT(offset < m_size);
			return m_data[offset];
		}

		value_type* data() const { return m_data; }

		bool empty() const { return m_size == 0; }
		size_type size() const { return m_size; }

		void clear()
		{
			m_data = nullptr;
			m_size = 0;
		}

	private:
		value_type* m_data { nullptr };
		size_type m_size { 0 };

		friend class ByteSpanGeneral<!CONST>;
	};

	using ByteSpan = ByteSpanGeneral<false>;
	using ConstByteSpan = ByteSpanGeneral<true>;

}
