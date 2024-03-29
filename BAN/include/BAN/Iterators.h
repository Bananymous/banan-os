#pragma once

#include <BAN/Assert.h>

namespace BAN
{

	template<typename T, typename Container, bool CONST>
	class IteratorSimpleGeneral
	{
	public:
		IteratorSimpleGeneral() = default;
		template<bool CONST2, typename = enable_if_t<CONST2 == CONST || CONST>>
		IteratorSimpleGeneral(const IteratorSimpleGeneral<T, Container, CONST2>& other)
			: m_pointer(other.m_pointer)
		{
		}

		const T& operator*() const
		{
			ASSERT(m_pointer);
			return *m_pointer;
		}
		template<bool CONST2 = CONST>
		enable_if_t<!CONST2, T&> operator*()
		{
			ASSERT(m_pointer);
			return *m_pointer;
		}

		const T* operator->() const
		{
			ASSERT(m_pointer);
			return m_pointer;
		}
		template<bool CONST2 = CONST>
		enable_if_t<!CONST2, T*> operator->()
		{
			ASSERT(m_pointer);
			return m_pointer;
		}

		IteratorSimpleGeneral& operator++()
		{
			ASSERT(m_pointer);
			++m_pointer;
			return *this;
		}
		IteratorSimpleGeneral operator++(int)
		{
			auto temp = *this;
			++(*this);
			return temp;
		}

		IteratorSimpleGeneral& operator--()
		{
			ASSERT(m_pointer);
			return --m_pointer;
		}
		IteratorSimpleGeneral operator--(int)
		{
			auto temp = *this;
			--(*this);
			return temp;
		}

		bool operator==(const IteratorSimpleGeneral& other) const
		{
			return m_pointer == other.m_pointer;
		}
		bool operator!=(const IteratorSimpleGeneral& other) const
		{
			return !(*this == other);
		}

		operator bool() const
		{
			return m_pointer;
		}

	private:
		IteratorSimpleGeneral(maybe_const_t<CONST, T>* pointer)
			: m_pointer(pointer)
		{
		}

	private:
		maybe_const_t<CONST, T>* m_pointer = nullptr;

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

	public:
		IteratorDoubleGeneral() = default;
		template<bool CONST2, typename = enable_if_t<CONST2 == CONST || CONST>>
		IteratorDoubleGeneral(const IteratorDoubleGeneral<T, OuterContainer, InnerContainer, Container, CONST2>& other)
			: m_outer_end(other.m_outer_end)
			, m_outer_current(other.m_outer_current)
			, m_inner_current(other.m_inner_current)
		{
		}

		const T& operator*() const
		{
			ASSERT(*this);
			ASSERT(m_outer_current != m_outer_end);
			ASSERT(m_inner_current);
			return m_inner_current.operator*();
		}
		template<bool CONST2 = CONST>
		enable_if_t<!CONST2, T&> operator*()
		{
			ASSERT(*this);
			ASSERT(m_outer_current != m_outer_end);
			ASSERT(m_inner_current);
			return m_inner_current.operator*();
		}

		const T* operator->() const
		{
			ASSERT(*this);
			ASSERT(m_outer_current != m_outer_end);
			ASSERT(m_inner_current);
			return m_inner_current.operator->();
		}
		template<bool CONST2 = CONST>
		enable_if_t<!CONST2, T*> operator->()
		{
			ASSERT(*this);
			ASSERT(m_outer_current != m_outer_end);
			ASSERT(m_inner_current);
			return m_inner_current.operator->();
		}

		IteratorDoubleGeneral& operator++()
		{
			ASSERT(*this);
			ASSERT(m_outer_current != m_outer_end);
			ASSERT(m_inner_current);
			m_inner_current++;
			find_valid_or_end();
			return *this;
		}
		IteratorDoubleGeneral operator++(int)
		{
			auto temp = *this;
			++(*this);
			return temp;
		}

		bool operator==(const IteratorDoubleGeneral& other) const
		{
			if (!*this || !other)
				return false;
			if (m_outer_end != other.m_outer_end)
				return false;
			if (m_outer_current != other.m_outer_current)
				return false;
			if (m_outer_current == m_outer_end)
				return true;
			return m_inner_current == other.m_inner_current;
		}
		bool operator!=(const IteratorDoubleGeneral& other) const
		{
			return !(*this == other);
		}

		operator bool() const
		{
			return m_outer_end && m_outer_current;
		}

	private:
		IteratorDoubleGeneral(const OuterIterator& outer_end, const OuterIterator& outer_current)
			: m_outer_end(outer_end)
			, m_outer_current(outer_current)
		{
			if (outer_current != outer_end)
			{
				m_inner_current = m_outer_current->begin();
				find_valid_or_end();
			}
		}

		void find_valid_or_end()
		{
			while (m_inner_current == m_outer_current->end())
			{
				m_outer_current++;
				if (m_outer_current == m_outer_end)
					break;
				m_inner_current = m_outer_current->begin();
			}
		}

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
