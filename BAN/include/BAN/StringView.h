#pragma once

#include <BAN/Formatter.h>
#include <BAN/ForwardList.h>
#include <BAN/Iterators.h>
#include <BAN/Optional.h>
#include <BAN/Vector.h>

namespace BAN
{

	class StringView
	{
	public:
		using size_type = size_t;
		using const_iterator = ConstIteratorSimple<char, StringView>;

	public:
		constexpr StringView() {}
		constexpr StringView(const char* string, size_type len = -1)
		{
			if (len == size_type(-1))
				len = strlen(string);
			m_data = string;
			m_size = len;
		}
		StringView(const String&);

		constexpr const_iterator begin() const { return const_iterator(m_data); }
		constexpr const_iterator end() const { return const_iterator(m_data + m_size); }

		constexpr char operator[](size_type index) const
		{
			ASSERT(index < m_size);
			return m_data[index];
		}

		constexpr bool operator==(StringView other) const
		{
			if (m_size != other.m_size)
				return false;
			for (size_type i = 0; i < m_size; i++)
				if (m_data[i] != other.m_data[i])
					return false;
			return true;
		}

		constexpr bool operator==(const char* other) const
		{
			for (size_type i = 0; i < m_size; i++)
				if (m_data[i] != other[i])
					return false;
			return other[m_size] == '\0';
		}

		constexpr StringView substring(size_type index, size_type len = -1) const
		{
			ASSERT(index <= m_size);
			if (len == size_type(-1))
				len = m_size - index;
			ASSERT(len <= m_size - index); // weird order to avoid overflow
			StringView result;
			result.m_data = m_data + index;
			result.m_size = len;
			return result;
		}

		ErrorOr<Vector<StringView>> split(char delim, bool allow_empties = false) const
		{
			size_type count = 0;
			{
				size_type start = 0;
				for (size_type i = 0; i < m_size; i++)
				{
					if (m_data[i] == delim)
					{
						if (allow_empties || start != i)
							count++;
						start = i + 1;
					}
				}
				if (start != m_size)
					count++;
			}

			Vector<StringView> result;
			TRY(result.reserve(count));

			size_type start = 0;
			for (size_type i = 0; i < m_size; i++)
			{
				if (m_data[i] == delim)
				{
					if (allow_empties || start != i)
						TRY(result.push_back(this->substring(start, i - start)));
					start = i + 1;
				}
			}
			if (start < m_size || (start == m_size && allow_empties))
				TRY(result.push_back(this->substring(start)));
			return result;
		}

		ErrorOr<Vector<StringView>> split(bool(*comp)(char), bool allow_empties = false) const
		{
			size_type count = 0;
			{
				size_type start = 0;
				for (size_type i = 0; i < m_size; i++)
				{
					if (comp(m_data[i]))
					{
						if (allow_empties || start != i)
							count++;
						start = i + 1;
					}
				}
				if (start != m_size)
					count++;
			}

			Vector<StringView> result;
			TRY(result.reserve(count));

			size_type start = 0;
			for (size_type i = 0; i < m_size; i++)
			{
				if (comp(m_data[i]))
				{
					if (allow_empties || start != i)
						TRY(result.push_back(this->substring(start, i - start)));
					start = i + 1;
				}
			}
			if (start < m_size || (start == m_size && allow_empties))
				TRY(result.push_back(this->substring(start)));
			return result;
		}

		constexpr char back() const
		{
			ASSERT(m_size > 0);
			return m_data[m_size - 1];
		}

		constexpr char front() const
		{
			ASSERT(m_size > 0);
			return m_data[0];
		}

		BAN::Optional<size_type> find(char ch) const
		{
			for (size_type i = 0; i < m_size; i++)
				if (m_data[i] == ch)
					return i;
			return {};
		}

		BAN::Optional<size_type> find(bool(*comp)(char)) const
		{
			for (size_type i = 0; i < m_size; i++)
				if (comp(m_data[i]))
					return i;
			return {};
		}

		constexpr bool contains(char ch) const
		{
			for (size_type i = 0; i < m_size; i++)
				if (m_data[i] == ch)
					return true;
			return false;
		}

		constexpr size_type count(char ch) const
		{
			size_type result = 0;
			for (size_type i = 0; i < m_size; i++)
				if (m_data[i] == ch)
					result++;
			return result;
		}

		constexpr bool empty() const { return m_size == 0; }
		constexpr size_type size() const { return m_size; }
		constexpr const char* data() const { return m_data; }

	private:
		const char*	m_data = nullptr;
		size_type	m_size = 0;
	};

}

inline constexpr BAN::StringView operator""_sv(const char* str, BAN::StringView::size_type len) { return BAN::StringView(str, len); }

namespace BAN::Formatter
{

	template<typename F>
	void print_argument(F putc, const StringView& sv, const ValueFormat&)
	{
		for (StringView::size_type i = 0; i < sv.size(); i++)
			putc(sv[i]);
	}

}
