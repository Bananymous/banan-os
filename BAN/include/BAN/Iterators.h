#pragma once

#include <BAN/Assert.h>
#include <BAN/Traits.h>

#include <stddef.h>

namespace BAN
{

	template<typename It>
	constexpr It next(It it, size_t count)
	{
		for (size_t i = 0; i < count; i++)
			++it;
		return it;
	}

	template<typename It>
	requires requires(It it, size_t n) { requires is_same_v<decltype(it + n), It>; }
	constexpr It next(It it, size_t count)
	{
		return it + count;
	}

	template<typename It>
	constexpr It prev(It it, size_t count)
	{
		for (size_t i = 0; i < count; i++)
			--it;
		return it;
	}

	template<typename It>
	requires requires(It it, size_t n) { requires is_same_v<decltype(it - n), It>; }
	constexpr It prev(It it, size_t count)
	{
		return it - count;
	}

	template<typename It>
	constexpr size_t distance(It it1, It it2)
	{
		size_t dist = 0;
		while (it1 != it2)
		{
			++it1;
			++dist;
		}
		return dist;
	}

	template<typename It>
	requires requires(It it1, It it2) { requires is_integral_v<decltype(it2 - it1)>; }
	constexpr size_t distance(It it1, It it2)
	{
		return it2 - it1;
	}

	template<typename T, typename Container, bool CONST>
	class IteratorSimpleGeneral
	{
	public:
		using value_type = T;

	public:
		constexpr IteratorSimpleGeneral() = default;
		template<bool CONST2, typename = enable_if_t<CONST2 == CONST || CONST>>
		constexpr IteratorSimpleGeneral(const IteratorSimpleGeneral<T, Container, CONST2>& other)
			: m_pointer(other.m_pointer)
			, m_valid(other.m_valid)
		{
		}

		constexpr const T& operator*() const
		{
			ASSERT(m_pointer);
			return *m_pointer;
		}
		template<bool CONST2 = CONST>
		constexpr enable_if_t<!CONST2, T&> operator*()
		{
			ASSERT(*this);
			ASSERT(m_pointer);
			return *m_pointer;
		}

		constexpr const T* operator->() const
		{
			ASSERT(*this);
			ASSERT(m_pointer);
			return m_pointer;
		}
		template<bool CONST2 = CONST>
		constexpr enable_if_t<!CONST2, T*> operator->()
		{
			ASSERT(*this);
			ASSERT(m_pointer);
			return m_pointer;
		}

		constexpr IteratorSimpleGeneral& operator++()
		{
			ASSERT(*this);
			ASSERT(m_pointer);
			++m_pointer;
			return *this;
		}
		constexpr IteratorSimpleGeneral operator++(int)
		{
			auto temp = *this;
			++(*this);
			return temp;
		}

		constexpr IteratorSimpleGeneral& operator--()
		{
			ASSERT(*this);
			ASSERT(m_pointer);
			--m_pointer;
			return *this;
		}
		constexpr IteratorSimpleGeneral operator--(int)
		{
			auto temp = *this;
			--(*this);
			return temp;
		}

		constexpr size_t operator-(const IteratorSimpleGeneral& other) const
		{
			ASSERT(*this && other);
			return m_pointer - other.m_pointer;
		}

		constexpr IteratorSimpleGeneral operator+(size_t offset) const
		{
			return IteratorSimpleGeneral(m_pointer + offset);
		}

		constexpr IteratorSimpleGeneral operator-(size_t offset) const
		{
			return IteratorSimpleGeneral(m_pointer - offset);
		}

		constexpr bool operator<(const IteratorSimpleGeneral& other) const
		{
			ASSERT(*this);
			return m_pointer < other.m_pointer;
		}

		constexpr bool operator==(const IteratorSimpleGeneral& other) const
		{
			ASSERT(*this);
			return m_pointer == other.m_pointer;
		}
		constexpr bool operator!=(const IteratorSimpleGeneral& other) const
		{
			ASSERT(*this);
			return !(*this == other);
		}

		constexpr explicit operator bool() const
		{
			return m_valid;
		}

	private:
		constexpr IteratorSimpleGeneral(maybe_const_t<CONST, T>* pointer)
			: m_pointer(pointer)
			, m_valid(true)
		{
		}

	private:
		maybe_const_t<CONST, T>* m_pointer = nullptr;
		bool m_valid = false;

		friend IteratorSimpleGeneral<T, Container, !CONST>;
		friend Container;
	};

	template<typename T, template<typename> typename OuterContainer, template<typename> typename InnerContainer, typename Container, bool CONST>
	class IteratorDoubleGeneral
	{
	public:
		using Inner = InnerContainer<T>;
		using Outer = OuterContainer<Inner>;

		using InnerIterator = either_or_t<CONST, typename Inner::const_iterator, typename Inner::iterator>;
		using OuterIterator = either_or_t<CONST, typename Outer::const_iterator, typename Outer::iterator>;

		using value_type = T;

	public:
		constexpr IteratorDoubleGeneral() = default;
		template<bool CONST2, typename = enable_if_t<CONST2 == CONST || CONST>>
		constexpr IteratorDoubleGeneral(const IteratorDoubleGeneral<T, OuterContainer, InnerContainer, Container, CONST2>& other)
			: m_outer_end(other.m_outer_end)
			, m_outer_current(other.m_outer_current)
			, m_inner_current(other.m_inner_current)
		{
		}

		constexpr const T& operator*() const
		{
			ASSERT(*this);
			ASSERT(m_outer_current != m_outer_end);
			ASSERT(m_inner_current);
			return m_inner_current.operator*();
		}
		template<bool CONST2 = CONST>
		constexpr enable_if_t<!CONST2, T&> operator*()
		{
			ASSERT(*this);
			ASSERT(m_outer_current != m_outer_end);
			ASSERT(m_inner_current);
			return m_inner_current.operator*();
		}

		constexpr const T* operator->() const
		{
			ASSERT(*this);
			ASSERT(m_outer_current != m_outer_end);
			ASSERT(m_inner_current);
			return m_inner_current.operator->();
		}
		template<bool CONST2 = CONST>
		constexpr enable_if_t<!CONST2, T*> operator->()
		{
			ASSERT(*this);
			ASSERT(m_outer_current != m_outer_end);
			ASSERT(m_inner_current);
			return m_inner_current.operator->();
		}

		constexpr IteratorDoubleGeneral& operator++()
		{
			ASSERT(*this);
			ASSERT(m_outer_current != m_outer_end);
			ASSERT(m_inner_current);
			m_inner_current++;
			find_valid_or_end();
			return *this;
		}
		constexpr IteratorDoubleGeneral operator++(int)
		{
			auto temp = *this;
			++(*this);
			return temp;
		}

		constexpr bool operator==(const IteratorDoubleGeneral& other) const
		{
			ASSERT(*this && other);
			if (m_outer_end != other.m_outer_end)
				return false;
			if (m_outer_current != other.m_outer_current)
				return false;
			if (m_outer_current == m_outer_end)
				return true;
			ASSERT(m_inner_current && other.m_inner_current);
			return m_inner_current == other.m_inner_current;
		}
		constexpr bool operator!=(const IteratorDoubleGeneral& other) const
		{
			return !(*this == other);
		}

		constexpr explicit operator bool() const
		{
			return !!m_outer_current;
		}

	private:
		constexpr IteratorDoubleGeneral(const OuterIterator& outer_end, const OuterIterator& outer_current)
			: m_outer_end(outer_end)
			, m_outer_current(outer_current)
		{
			if (outer_current != outer_end)
			{
				m_inner_current = m_outer_current->begin();
				find_valid_or_end();
			}
		}

		constexpr IteratorDoubleGeneral(const OuterIterator& outer_end, const OuterIterator& outer_current, const InnerIterator& inner_current)
			: m_outer_end(outer_end)
			, m_outer_current(outer_current)
			, m_inner_current(inner_current)
		{
			find_valid_or_end();
		}

		constexpr void find_valid_or_end()
		{
			while (m_inner_current == m_outer_current->end())
			{
				m_outer_current++;
				if (m_outer_current == m_outer_end)
					break;
				m_inner_current = m_outer_current->begin();
			}
		}

		constexpr OuterIterator outer_current() { return m_outer_current; }
		constexpr InnerIterator inner_current() { return m_inner_current; }

	private:
		OuterIterator m_outer_end;
		OuterIterator m_outer_current;
		InnerIterator m_inner_current;

		friend class IteratorDoubleGeneral<T, OuterContainer, InnerContainer, Container, !CONST>;
		friend Container;
	};

	template<typename T, typename Container>
	using IteratorSimple = IteratorSimpleGeneral<T, Container, false>;

	template<typename T, typename Container>
	using ConstIteratorSimple = IteratorSimpleGeneral<T, Container, true>;

	template<typename T, template<typename> typename OuterContainer, template<typename> typename InnerContainer, typename Container>
	using IteratorDouble = IteratorDoubleGeneral<T, OuterContainer, InnerContainer, Container, false>;

	template<typename T, template<typename> typename OuterContainer, template<typename> typename InnerContainer, typename Container>
	using ConstIteratorDouble = IteratorDoubleGeneral<T, OuterContainer, InnerContainer, Container, true>;

}
