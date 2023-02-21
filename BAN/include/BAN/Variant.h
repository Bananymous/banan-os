#pragma once

#include <BAN/Assert.h>
#include <BAN/Math.h>
#include <BAN/Move.h>

namespace BAN
{

	template<typename T1, typename T2>
	class Variant
	{
	public:
		static_assert(!is_same_v<T1, T2>);

		Variant() = default;

		Variant(const T1& value)	{ set(value); }
		Variant(T1&& value)			{ set(move(value)); }
		Variant(const T2& value)	{ set(value); }
		Variant(T2&& value)			{ set(move(value)); }

		Variant(const Variant<T1, T2>& other)	{ *this = other; }
		Variant(Variant<T1, T2>&& other)		{ *this = move(other); }

		~Variant() { clear(); }

		Variant<T1, T2>& operator=(const Variant<T1, T2>& other);
		Variant<T1, T2>& operator=(Variant<T1, T2>&& other);

		template<typename U>
		bool is() const;

		template<typename U>
		void set(U&&);
		template<typename U>
		void set(const U& value) { set(move(U(value))); } 

		template<typename U>
		const U& get() const;
		template<typename U>
		U& get();

		void clear();

	private:
		static constexpr uint32_t m_size = Math::max(sizeof(T1), sizeof(T2));
		uint8_t m_storage[m_size] = {};
		uint32_t m_index = 0;
	};

	template<typename T1, typename T2>
	Variant<T1, T2>& Variant<T1, T2>::operator=(const Variant<T1, T2>& other)
	{
		clear();
		if (other.is<T1>())
			set(other.get<T1>());
		if (other.is<T2>())
			set(other.get<T2>());
		return *this;
	}

	template<typename T1, typename T2>
	Variant<T1, T2>& Variant<T1, T2>::operator=(Variant<T1, T2>&& other)
	{
		clear();
		if (other.is<T1>())
			set(move(other.get<T1>()));
		if (other.is<T2>())
			set(move(other.get<T2>()));
		other.clear();
		return *this;
	}

	template<typename T1, typename T2>
	template<typename U>
	bool Variant<T1, T2>::is() const
	{
		if constexpr(is_same_v<T1, U>)
			return m_index == 1;
		if constexpr(is_same_v<T2, U>)
			return m_index == 2;
		return false;
	}


	template<typename T1, typename T2>
	template<typename U>
	void Variant<T1, T2>::set(U&& value)
	{
		static_assert(is_same_v<T1, U> || is_same_v<T2, U>);
		clear();
		if constexpr(is_same_v<T1, U>)
		{
			new (m_storage) T1(move(value));
			m_index = 1;
		}
		if constexpr(is_same_v<T2, U>)
		{
			new (m_storage) T2(move(value));
			m_index = 2;
		}
	}

	template<typename T1, typename T2>
	template<typename U>
	const U& Variant<T1, T2>::get() const
	{
		static_assert(is_same_v<T1, U> || is_same_v<T2, U>);
		if constexpr(is_same_v<T1, U>)
		{
			ASSERT(m_index == 1);
			return *(T1*)m_storage;
		}
		if constexpr(is_same_v<T2, U>)
		{
			ASSERT(m_index == 2);
			return *(T2*)m_storage;
		}
	}

	template<typename T1, typename T2>
	template<typename U>
	U& Variant<T1, T2>::get()
	{
		static_assert(is_same_v<T1, U> || is_same_v<T2, U>);
		if constexpr(is_same_v<T1, U>)
		{
			ASSERT(m_index == 1);
			return *(T1*)m_storage;
		}
		if constexpr(is_same_v<T2, U>)
		{
			ASSERT(m_index == 2);
			return *(T2*)m_storage;
		}
	}

	template<typename T1, typename T2>
	void Variant<T1, T2>::clear()
	{
		if (is<T1>()) ((T1*)m_storage)->~T1();
		if (is<T2>()) ((T2*)m_storage)->~T2();
		m_index = 0;
	}

}