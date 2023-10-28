#include <BAN/String.h>
#include <BAN/New.h>
#include <BAN/Variant.h>

namespace BAN
{

	String::String()
	{
	}

	String::String(const String& other)
	{
		*this = other;
	}

	String::String(String&& other)
	{
		*this = move(other);
	}

	String::String(StringView other)
	{
		*this = other;
	}

	String::~String()
	{
		clear();
	}

	String& String::operator=(const String& other)
	{
		clear();
		if (!other.fits_in_sso())
			MUST(ensure_capacity(other.size()));
		memcpy(data(), other.data(), other.size() + 1);
		m_size = other.size();
		return *this;
	}

	String& String::operator=(String&& other)
	{
		clear();

		if (other.fits_in_sso())
			memcpy(data(), other.data(), other.size() + 1);
		else
			m_storage = other.m_storage.get<GeneralStorage>();
		m_size = other.m_size;

		other.m_size = 0;
		other.m_storage = SSOStorage();

		return *this;
	}

	String& String::operator=(StringView other)
	{
		clear();
		if (!fits_in_sso(other.size()))
			MUST(ensure_capacity(other.size()));
		memcpy(data(), other.data(), other.size());
		m_size = other.size();
		data()[m_size] = '\0';
		return *this;
	}

	ErrorOr<void> String::push_back(char c)
	{
		TRY(ensure_capacity(m_size + 1));
		data()[m_size] = c;
		m_size++;
		data()[m_size] = '\0';
		return {};
	}

	ErrorOr<void> String::insert(char c, size_type index)
	{
		ASSERT(index <= m_size);
		TRY(ensure_capacity(m_size + 1));
		memmove(data() + index + 1, data() + index, m_size - index);
		data()[index] = c;
		m_size++;
		data()[m_size] = '\0';
		return {};
	}

	ErrorOr<void> String::insert(StringView str, size_type index)
	{
		ASSERT(index <= m_size);
		TRY(ensure_capacity(m_size + str.size()));
		memmove(data() + index + str.size(), data() + index, m_size - index);
		memcpy(data() + index, str.data(), str.size());
		m_size += str.size();
		data()[m_size] = '\0';
		return {};
	}

	ErrorOr<void> String::append(StringView str)
	{
		TRY(ensure_capacity(m_size + str.size()));
		memcpy(data() + m_size, str.data(), str.size());
		m_size += str.size();
		data()[m_size] = '\0';
		return {};
	}

	void String::pop_back()
	{
		ASSERT(m_size > 0);
		m_size--;
		data()[m_size] = '\0';
	}

	void String::remove(size_type index)
	{
		ASSERT(index < m_size);
		memcpy(data() + index, data() + index + 1, m_size - index);
		m_size--;
		data()[m_size] = '\0';
	}

	void String::clear()
	{
		if (!has_sso())
		{
			deallocator(m_storage.get<GeneralStorage>().data);
			m_storage = SSOStorage();
		}
		m_size = 0;
		data()[m_size] = '\0';
	}

	bool String::operator==(StringView str) const
	{
		if (size() != str.size())
			return false;
		for (size_type i = 0; i < m_size; i++)
			if (data()[i] != str.data()[i])
				return false;
		return true;
	}

	bool String::operator==(const char* cstr) const
	{
		for (size_type i = 0; i < m_size; i++)
			if (data()[i] != cstr[i])
				return false;
		if (cstr[size()] != '\0')
			return false;
		return true;
	}

	ErrorOr<void> String::resize(size_type new_size, char init_c)
	{
		if (m_size == new_size)
			return {};

		// expanding
		if (m_size < new_size)
		{
			TRY(ensure_capacity(new_size));
			memset(data() + m_size, init_c, new_size - m_size);
			m_size = new_size;
			data()[m_size] = '\0';
			return {};
		}
		
		// shrink general -> sso
		if (!has_sso() && fits_in_sso(new_size))
		{
			char* data = m_storage.get<GeneralStorage>().data;
			m_storage = SSOStorage();
			memcpy(m_storage.get<SSOStorage>().storage, data, new_size);
			deallocator(data);
		}

		m_size = new_size;
		data()[m_size] = '\0';
		return {};
	}

	ErrorOr<void> String::reserve(size_type new_size)
	{
		TRY(ensure_capacity(new_size));
		return {};
	}

	ErrorOr<void> String::shrink_to_fit()
	{
		if (has_sso())
			return {};

		if (fits_in_sso())
		{
			char* data = m_storage.get<GeneralStorage>().data;
			m_storage = SSOStorage();
			memcpy(m_storage.get<SSOStorage>().storage, data, m_size + 1);
			deallocator(data);
			return {};
		}

		GeneralStorage& storage = m_storage.get<GeneralStorage>();
		if (storage.capacity == m_size)
			return {};

		char* new_data = (char*)allocator(m_size + 1);
		if (new_data == nullptr)
			return BAN::Error::from_errno(ENOMEM);

		memcpy(new_data, storage.data, m_size);
		deallocator(storage.data);

		storage.capacity = m_size;
		storage.data = new_data;

		return {};
	}

	String::size_type String::capacity() const
	{
		if (has_sso())
			return sso_capacity;
		return m_storage.get<GeneralStorage>().capacity;
	}

	char* String::data()
	{
		if (has_sso())
			return m_storage.get<SSOStorage>().storage;
		return m_storage.get<GeneralStorage>().data;
	}

	const char* String::data() const
	{
		if (has_sso())
			return m_storage.get<SSOStorage>().storage;
		return m_storage.get<GeneralStorage>().data;
	}

	ErrorOr<void> String::ensure_capacity(size_type new_size)
	{
		if (m_size >= new_size || fits_in_sso(new_size))
			return {};
		
		char* new_data = (char*)allocator(new_size + 1);
		if (new_data == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		
		memcpy(new_data, data(), m_size + 1);

		if (has_sso())
			m_storage = GeneralStorage();
		else
			deallocator(m_storage.get<GeneralStorage>().data);

		auto& storage = m_storage.get<GeneralStorage>();
		storage.capacity = new_size;
		storage.data = new_data;

		return {};
	}

	bool String::has_sso() const
	{
		return m_storage.has<SSOStorage>();
	}

}
