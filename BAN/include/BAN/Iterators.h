#pragma once

#include <BAN/Assert.h>

namespace BAN
{

	template<typename T, typename Container>
	class IteratorSimple
	{
	public:
		IteratorSimple() = default;

		const T& operator*() const
		{
			ASSERT(m_pointer);
			return *m_pointer;
		}
		T& operator*()
		{
			ASSERT(m_pointer);
			return *m_pointer;
		}

		const T* operator->() const
		{
			ASSERT(m_pointer);
			return m_pointer;
		}
		T* operator->()
		{
			ASSERT(m_pointer);
			return m_pointer;
		}

		IteratorSimple& operator++()
		{
			ASSERT(m_pointer);
			++m_pointer;
			return *this;
		}
		IteratorSimple operator++(int)
		{
			auto temp = *this;
			++(*this);
			return temp;
		}

		IteratorSimple& operator--()
		{
			ASSERT(m_pointer);
			return --m_pointer;
		}
		IteratorSimple operator--(int)
		{
			auto temp = *this;
			--(*this);
			return temp;
		}

		bool operator==(const IteratorSimple& other) const
		{
			return m_pointer == other.m_pointer;
		}
		bool operator!=(const IteratorSimple& other) const
		{
			return !(*this == other);
		}

		operator bool() const
		{
			return m_pointer;
		}

	private:
		IteratorSimple(T* pointer)
			: m_pointer(pointer)
		{
		}

	private:
		T* m_pointer = nullptr;

		friend Container;
	};

	template<typename T, typename Container>
	class ConstIteratorSimple
	{
	public:
		ConstIteratorSimple() = default;
		ConstIteratorSimple(IteratorSimple<T, Container> other)
			: m_pointer(other.m_pointer)
		{
		}

		const T& operator*() const
		{
			ASSERT(m_pointer);
			return *m_pointer;
		}

		const T* operator->() const
		{
			ASSERT(m_pointer);
			return m_pointer;
		}

		ConstIteratorSimple& operator++()
		{
			ASSERT(m_pointer);
			++m_pointer;
			return *this;
		}
		ConstIteratorSimple operator++(int)
		{
			auto temp = *this;
			++(*this);
			return temp;
		}

		ConstIteratorSimple& operator--()
		{
			ASSERT(m_pointer);
			return --m_pointer;
		}
		ConstIteratorSimple operator--(int)
		{
			auto temp = *this;
			--(*this);
			return temp;
		}

		bool operator==(const ConstIteratorSimple& other) const
		{
			return m_pointer == other.m_pointer;
		}
		bool operator!=(const ConstIteratorSimple& other) const
		{
			return !(*this == other);
		}

		operator bool() const
		{
			return !!m_pointer;
		}

	private:
		ConstIteratorSimple(const T* pointer)
			: m_pointer(pointer)
		{
		}

	private:
		const T* m_pointer = nullptr;

		friend Container;
	};

	template<typename T, template <typename> typename OuterContainer, template <typename> typename InnerContainer, typename Container>
	class IteratorDouble
	{
	public:
		using Inner = InnerContainer<T>;
		using Outer = OuterContainer<Inner>;

	public:
		IteratorDouble() = default;

		const T& operator*() const
		{
			ASSERT(*this);
			ASSERT(m_outer_current != m_outer_end);
			ASSERT(m_inner_current);
			return m_inner_current.operator*();
		}
		T& operator*()
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
		T* operator->()
		{
			ASSERT(*this);
			ASSERT(m_outer_current != m_outer_end);
			ASSERT(m_inner_current);
			return m_inner_current.operator->();
		}

		IteratorDouble& operator++()
		{
			ASSERT(*this);
			ASSERT(m_outer_current != m_outer_end);
			ASSERT(m_inner_current);
			m_inner_current++;
			find_valid_or_end();
			return *this;
		}
		IteratorDouble operator++(int)
		{
			auto temp = *this;
			++(*this);
			return temp;
		}

		bool operator==(const IteratorDouble& other) const
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
		bool operator!=(const IteratorDouble& other) const
		{
			return !(*this == other);
		}

		operator bool() const
		{
			return m_outer_end && m_outer_current;
		}

	private:
		IteratorDouble(Outer::iterator outer_end, Outer::iterator outer_current)
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
		Outer::iterator m_outer_end;
		Outer::iterator m_outer_current;
		Inner::iterator m_inner_current;

		friend Container;
	};

}
