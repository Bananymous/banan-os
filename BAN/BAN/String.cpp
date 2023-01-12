#include <BAN/Errors.h>
#include <BAN/Math.h>
#include <BAN/Memory.h>
#include <BAN/Move.h>
#include <BAN/String.h>
#include <BAN/StringView.h>

#include <string.h>

namespace BAN
{

	String::String()
	{
		MUST(copy_impl("", 0));
	}

	String::String(const String& other)
	{
		MUST(copy_impl(other.Data(), other.Size()));
	}

	String::String(String&& other)
	{
		move_impl(Move(other));
	}

	String::String(const StringView& other)
	{
		MUST(copy_impl(other.Data(), other.Size()));
	}

	String::String(const char* data, size_type len)
	{
		if (len == size_type(-1))
			len = strlen(data);
		MUST(copy_impl(data, len));
	}

	String::~String()
	{
		BAN::deallocator(m_data);
	}

	String& String::operator=(const String& other)
	{
		copy_impl(other.Data(), other.Size());
		return *this;
	}

	String& String::operator=(String&& other)
	{
		BAN::deallocator(m_data);
		move_impl(Move(other));
		return *this;
	}

	ErrorOr<void> String::PushBack(char ch)
	{
		TRY(EnsureCapasity(m_size + 2));
		m_data[m_size]		= ch;
		m_data[m_size + 1]	= '\0';
		m_size++;
		return {};
	}

	ErrorOr<void> String::Insert(char ch, size_type index)
	{
		ASSERT(index <= m_size);
		TRY(EnsureCapasity(m_size + 2));
		memmove(m_data + index + 1, m_data + index, m_size - index);
		m_data[index]		= ch;
		m_data[m_size + 1]	= '\0';
		m_size++;
		return {};
	}

	ErrorOr<void> String::Append(const char* string)
	{
		size_t len = strlen(string);
		TRY(EnsureCapasity(m_size + len + 1));
		memcpy(m_data + m_size, string, len);
		m_data[m_size + len] = '\0';
		m_size += len;
		return {};
	}

	ErrorOr<void> String::Append(const String& string)
	{
		TRY(Append(string.Data()));
		return {};
	}

	void String::PopBack()
	{
		ASSERT(m_size > 0);
		m_data[m_size - 1] = '\0';
		m_size--;
	}

	void String::Remove(size_type index)
	{
		ASSERT(index < m_size);
		memmove(m_data + index, m_data + index + 1, m_size - index - 1);
		m_data[m_size - 1] = '\0';
		m_size--;
	}
	
	void String::Clear()
	{
		m_data[0] = '\0';
		m_size = 0;
	}

	char String::operator[](size_type index) const
	{
		ASSERT(index < m_size);
		return m_data[index];
	}

	char& String::operator[](size_type index)
	{
		ASSERT(index < m_size);
		return m_data[index];
	}

	bool String::operator==(const String& other) const
	{
		if (m_size != other.m_size)
			return false;
		return memcmp(m_data, other.m_data, m_size) == 0;
	}

	bool String::operator==(StringView other) const
	{
		if (m_size != other.Size())
			return false;
		return memcmp(m_data, other.Data(), m_size) == 0;
	}

	bool String::operator==(const char* other) const
	{
		if (memcmp(m_data, other, m_size))
			return false;
		return other[m_size] == '\0';
	}

	ErrorOr<void> String::Resize(size_type size, char ch)
	{
		if (size < m_size)
		{
			m_data[size] = '\0';
			m_size = size;
		}
		else if (size > m_size)
		{
			TRY(EnsureCapasity(size + 1));
			for (size_type i = m_size; i < size; i++)
				m_data[i] = ch;
			m_data[size] = '\0';
			m_size = size;
		}
		m_size = size;
		return {};
	}

	ErrorOr<void> String::Reserve(size_type size)
	{
		TRY(EnsureCapasity(size));
		return {};
	}

	StringView String::SV() const
	{
		return StringView(*this);
	}

	bool String::Empty() const
	{
		return m_size == 0;
	}

	String::size_type String::Size() const
	{
		return m_size;
	}

	String::size_type String::Capasity() const
	{
		return m_capasity;
	}

	const char* String::Data() const
	{
		return m_data;
	}

	ErrorOr<void> String::EnsureCapasity(size_type size)
	{
		if (m_capasity >= size)
			return {};
		size_type new_cap = BAN::Math::max<size_type>(size, m_capasity * 3 / 2);
		void* new_data = BAN::allocator(new_cap);
		if (new_data == nullptr)
			return Error::FromString("String: Could not allocate memory");
		memcpy(new_data, m_data, m_size + 1);
		BAN::deallocator(m_data);
		m_data = (char*)new_data;
		m_capasity = new_cap;
		return {};
	}

	ErrorOr<void> String::copy_impl(const char* data, size_type len)
	{
		TRY(EnsureCapasity(len + 1));
		memcpy(m_data, data, len);
		m_data[len] = '\0';
		m_size = len;
		return {};
	}

	void String::move_impl(String&& other)
	{
		m_data		= other.m_data;
		m_size		= other.m_size;
		m_capasity	= other.m_capasity;

		other.m_data = nullptr;
		other.m_size = 0;
		other.m_capasity = 0;
	}

}
