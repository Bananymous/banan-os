#include <BAN/String.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>

#include <string.h>

namespace BAN
{

	StringView::StringView()
	{ }

	StringView::StringView(const String& other)
		: StringView(other.data(), other.size())
	{ }

	StringView::StringView(const char* string, size_type len)
	{
		if (len == size_type(-1))
			len = strlen(string);
		m_data = string;
		m_size = len;
	}

	char StringView::operator[](size_type index) const
	{
		ASSERT(index < m_size);
		return m_data[index];
	}

	bool StringView::operator==(const String& other) const
	{
		if (m_size != other.size())
			return false;
		return memcmp(m_data, other.data(), m_size) == 0;
	}

	bool StringView::operator==(StringView other) const
	{
		if (m_size != other.m_size)
			return false;
		return memcmp(m_data, other.m_data, m_size) == 0;
	}

	bool StringView::operator==(const char* other) const
	{
		if (memcmp(m_data, other, m_size))
			return false;
		return other[m_size] == '\0';
	}

	StringView StringView::substring(size_type index, size_type len) const
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

	ErrorOr<Vector<StringView>> StringView::split(char delim, bool allow_empties)
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
		if (start != m_size)
			TRY(result.push_back(this->substring(start)));
		return result;
	}

	ErrorOr<Vector<StringView>> StringView::split(bool(*comp)(char), bool allow_empties)
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
		if (start != m_size)
			TRY(result.push_back(this->substring(start)));
		return result;
	}

	char StringView::back() const
	{
		ASSERT(m_size > 0);
		return m_data[m_size - 1];
	}

	char StringView::front() const
	{
		ASSERT(m_size > 0);
		return m_data[0];
	}

	StringView::size_type StringView::count(char ch) const
	{
		size_type result = 0;
		for (size_type i = 0; i < m_size; i++)
			if (m_data[i] == ch)
				result++;
		return result;
	}

	bool StringView::empty() const
	{
		return m_size == 0;
	}

	StringView::size_type StringView::size() const
	{
		return m_size;
	}
	
	const char* StringView::data() const
	{
		return m_data;
	}

}