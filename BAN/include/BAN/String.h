#pragma once

#include <BAN/Errors.h>
#include <BAN/Formatter.h>
#include <BAN/Hash.h>
#include <BAN/Iterators.h>
#include <BAN/New.h>
#include <BAN/StringView.h>

namespace BAN
{

	class String
	{
	public:
		using size_type = size_t;
		using iterator = IteratorSimple<char, String>;
		using const_iterator = ConstIteratorSimple<char, String>;
		static constexpr size_type sso_capacity = 15;

	public:
		String() {}
		String(const String& other) { *this = other; }
		String(String&& other) { *this = move(other); }
		String(StringView other) { *this = other; }
		~String() { clear(); }

		template<typename... Args>
		static BAN::ErrorOr<String> formatted(const char* format, Args&&... args)
		{
			size_type length = 0;
			BAN::Formatter::print([&](char) { length++; }, format, BAN::forward<Args>(args)...);

			String result;
			TRY(result.reserve(length));
			BAN::Formatter::print([&](char c){ MUST(result.push_back(c)); }, format, BAN::forward<Args>(args)...);

			return result;
		}

		String& operator=(const String& other)
		{
			clear();
			MUST(ensure_capacity(other.size()));
			memcpy(data(), other.data(), other.size() + 1);
			m_size = other.size();
			return *this;
		}

		String& operator=(String&& other)
		{
			clear();

			if (other.has_sso())
				memcpy(data(), other.data(), other.size() + 1);
			else
			{
				m_storage.general_storage = other.m_storage.general_storage;
				m_has_sso = false;
			}
			m_size = other.m_size;

			other.m_size = 0;
			other.m_storage.sso_storage = SSOStorage();
			other.m_has_sso = true;

			return *this;
		}

		String& operator=(StringView other)
		{
			clear();
			MUST(ensure_capacity(other.size()));
			memcpy(data(), other.data(), other.size());
			m_size = other.size();
			data()[m_size] = '\0';
			return *this;
		}

		ErrorOr<void> push_back(char c)
		{
			TRY(ensure_capacity(m_size + 1));
			data()[m_size] = c;
			m_size++;
			data()[m_size] = '\0';
			return {};
		}

		ErrorOr<void> insert(char c, size_type index)
		{
			ASSERT(index <= m_size);
			TRY(ensure_capacity(m_size + 1));
			memmove(data() + index + 1, data() + index, m_size - index);
			data()[index] = c;
			m_size++;
			data()[m_size] = '\0';
			return {};
		}

		ErrorOr<void> insert(StringView str, size_type index)
		{
			ASSERT(index <= m_size);
			TRY(ensure_capacity(m_size + str.size()));
			memmove(data() + index + str.size(), data() + index, m_size - index);
			memcpy(data() + index, str.data(), str.size());
			m_size += str.size();
			data()[m_size] = '\0';
			return {};
		}

		ErrorOr<void> append(StringView str)
		{
			TRY(ensure_capacity(m_size + str.size()));
			memcpy(data() + m_size, str.data(), str.size());
			m_size += str.size();
			data()[m_size] = '\0';
			return {};
		}

		void pop_back()
		{
			ASSERT(m_size > 0);
			m_size--;
			data()[m_size] = '\0';
		}

		void remove(size_type index)
		{
			ASSERT(index < m_size);
			memcpy(data() + index, data() + index + 1, m_size - index);
			m_size--;
			data()[m_size] = '\0';
		}

		void clear()
		{
			if (!has_sso())
			{
				deallocator(m_storage.general_storage.data);
				m_storage.sso_storage = SSOStorage();
				m_has_sso = true;
			}
			m_size = 0;
			data()[m_size] = '\0';
		}

		const_iterator begin() const	{ return const_iterator(data()); }
		iterator begin()				{ return iterator(data()); }
		const_iterator end() const		{ return const_iterator(data() + size()); }
		iterator end()					{ return iterator(data() + size()); }

		char front() const	{ ASSERT(m_size > 0); return data()[0]; }
		char& front()		{ ASSERT(m_size > 0); return data()[0]; }

		char back() const	{ ASSERT(m_size > 0); return data()[m_size - 1]; }
		char& back()		{ ASSERT(m_size > 0); return data()[m_size - 1]; }

		char operator[](size_type index) const	{ ASSERT(index < m_size); return data()[index]; }
		char& operator[](size_type index)		{ ASSERT(index < m_size); return data()[index]; }

		bool operator==(const String& str) const
		{
			if (size() != str.size())
				return false;
			for (size_type i = 0; i < m_size; i++)
				if (data()[i] != str.data()[i])
					return false;
			return true;
		}

		bool operator==(StringView str) const
		{
			if (size() != str.size())
				return false;
			for (size_type i = 0; i < m_size; i++)
				if (data()[i] != str.data()[i])
					return false;
			return true;
		}

		bool operator==(const char* cstr) const
		{
			for (size_type i = 0; i < m_size; i++)
				if (data()[i] != cstr[i])
					return false;
			if (cstr[size()] != '\0')
				return false;
			return true;
		}

		ErrorOr<void> resize(size_type new_size, char init_c = '\0')
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

			m_size = new_size;
			data()[m_size] = '\0';
			return {};
		}

		ErrorOr<void> reserve(size_type new_size)
		{
			TRY(ensure_capacity(new_size));
			return {};
		}

		ErrorOr<void> shrink_to_fit()
		{
			if (has_sso())
				return {};

			if (fits_in_sso())
			{
				char* data = m_storage.general_storage.data;
				m_storage.sso_storage = SSOStorage();
				m_has_sso = true;
				memcpy(this->data(), data, m_size + 1);
				deallocator(data);
				return {};
			}

			GeneralStorage& storage = m_storage.general_storage;
			if (storage.capacity == m_size)
				return {};

			char* new_data = (char*)allocator(m_size + 1);
			if (new_data == nullptr)
				return Error::from_errno(ENOMEM);

			memcpy(new_data, storage.data, m_size);
			deallocator(storage.data);

			storage.capacity = m_size;
			storage.data = new_data;

			return {};
		}

		StringView sv() const	{ return StringView(data(), size()); }

		bool empty() const		{ return m_size == 0; }
		size_type size() const	{ return m_size; }

		size_type capacity() const
		{
			if (has_sso())
				return sso_capacity;
			return m_storage.general_storage.capacity;
		}

		char* data()
		{
			if (has_sso())
				return m_storage.sso_storage.data;
			return m_storage.general_storage.data;
		}

		const char* data() const
		{
			if (has_sso())
				return m_storage.sso_storage.data;
			return m_storage.general_storage.data;
		}

	private:
		ErrorOr<void> ensure_capacity(size_type new_size)
		{
			if (m_size >= new_size)
				return {};
			if (has_sso() && fits_in_sso(new_size))
				return {};

			char* new_data = (char*)allocator(new_size + 1);
			if (new_data == nullptr)
				return Error::from_errno(ENOMEM);

			if (m_size)
				memcpy(new_data, data(), m_size + 1);

			if (has_sso())
			{
				m_storage.general_storage = GeneralStorage();
				m_has_sso = false;
			}
			else
				deallocator(m_storage.general_storage.data);

			auto& storage = m_storage.general_storage;
			storage.capacity = new_size;
			storage.data = new_data;

			return {};
		}

		bool has_sso() const { return m_has_sso; }

		bool fits_in_sso() const { return fits_in_sso(m_size); }
		static bool fits_in_sso(size_type size) { return size < sso_capacity; }

	private:
		struct SSOStorage
		{
			char data[sso_capacity + 1] {};
		};
		struct GeneralStorage
		{
			size_type capacity	{ 0 };
			char* data			{ nullptr };
		};

	private:
		union {
			SSOStorage sso_storage;
			GeneralStorage general_storage;
		} m_storage										{ .sso_storage = SSOStorage() };
		size_type m_size	: sizeof(size_type) * 8 - 1	{ 0 };
		size_type m_has_sso	: 1							{ true };
	};

	template<>
	struct hash<String>
	{
		hash_t operator()(const String& string) const
		{
			constexpr hash_t FNV_offset_basis = 0x811c9dc5;
			constexpr hash_t FNV_prime = 0x01000193;

			hash_t hash = FNV_offset_basis;
			for (String::size_type i = 0; i < string.size(); i++)
			{
				hash *= FNV_prime;
				hash ^= (uint8_t)string[i];
			}

			return hash;
		}
	};

}

namespace BAN::Formatter
{

	template<typename F>
	void print_argument(F putc, const String& string, const ValueFormat&)
	{
		for (String::size_type i = 0; i < string.size(); i++)
			putc(string[i]);
	}

}
