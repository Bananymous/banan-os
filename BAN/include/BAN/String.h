#pragma once

#include <BAN/Errors.h>
#include <BAN/Formatter.h>
#include <BAN/Memory.h>

#include <assert.h>
#include <string.h>
#include <sys/param.h>

namespace BAN
{

	class String
	{
	public:
		using size_type = size_t;

	public:
		String();
		String(const char*);
		~String();

		ErrorOr<void> PushBack(char);
		ErrorOr<void> Insert(char, size_type);
		ErrorOr<void> Append(const char*);
		ErrorOr<void> Append(const String&);
		
		void PopBack();
		void Remove(size_type);

		char operator[](size_type) const;
		char& operator[](size_type);

		ErrorOr<void> Resize(size_type, char = '\0');
		ErrorOr<void> Reserve(size_type);

		bool Empty() const;
		size_type Size() const;
		size_type Capasity() const;

		const char* Data() const;

	private:
		ErrorOr<void> EnsureCapasity(size_type);

	private:
		char*		m_data		= nullptr;
		size_type	m_capasity	= 0;
		size_type	m_size		= 0;	
	};

	String::String()
	{
		MUST(EnsureCapasity(1));
		m_data[0] = '\0';
		m_size = 0;
	}

	String::String(const char* string)
	{
		size_type len = strlen(string);
		MUST(EnsureCapasity(len + 1));
		memcpy(m_data, string, len);
		m_data[len] = '\0';
		m_size = len;
	}

	String::~String()
	{
		BAN::deallocator(m_data);
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
		assert(index <= m_size);
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
		assert(m_size > 0);
		m_data[m_size - 1] = '\0';
		m_size--;
	}

	void String::Remove(size_type index)
	{
		assert(index < m_size);
		memmove(m_data + index, m_data + index + 1, m_size - index - 1);
		m_data[m_size - 1] = '\0';
		m_size--;
	}
	
	char String::operator[](size_type index) const
	{
		assert(index < m_size);
		return m_data[index];
	}

	char& String::operator[](size_type index)
	{
		assert(index < m_size);
		return m_data[index];
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
		TRY(EnsureCapasity(size + 1));
		return {};
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
		size_type new_cap = MAX(size, m_capasity * 1.5f);
		void* new_data = BAN::allocator(new_cap);
		if (new_data == nullptr)
			return Error::FromString("String: Could not allocate memory");
		memcpy(new_data, m_data, m_size + 1);
		BAN::deallocator(m_data);
		m_data = (char*)new_data;
		m_capasity = new_cap;
		return {};
	}

}