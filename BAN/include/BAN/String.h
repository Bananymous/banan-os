#pragma once

#include <BAN/ForwardList.h>
#include <BAN/Formatter.h>
#include <BAN/Hash.h>
#include <BAN/Iterators.h>

namespace BAN
{

	class String
	{
	public:
		using size_type = size_t;
		using iterator = IteratorSimple<char, String>;
		using const_iterator = ConstIteratorSimple<char, String>;

	public:
		String();
		String(const String&);
		String(String&&);
		String(StringView);
		~String();

		template<typename... Args>
		static String formatted(const char* format, const Args&... args);

		String& operator=(const String&);
		String& operator=(String&&);
		String& operator=(StringView);

		ErrorOr<void> push_back(char);
		ErrorOr<void> insert(char, size_type);
		ErrorOr<void> insert(StringView, size_type);
		ErrorOr<void> append(StringView);
		ErrorOr<void> append(const String&);

		void pop_back();
		void remove(size_type);
		void erase(size_type, size_type);

		void clear();

		const_iterator begin() const { return const_iterator(m_data); }
		iterator begin() { return iterator(m_data); }
		const_iterator end() const { return const_iterator(m_data + m_size); }
		iterator end() { return iterator(m_data + m_size); }

		char front() const	{ ASSERT(!empty()); return m_data[0]; }
		char& front()		{ ASSERT(!empty()); return m_data[0]; }

		char back() const	{ ASSERT(!empty()); return m_data[m_size - 1]; }
		char& back()		{ ASSERT(!empty()); return m_data[m_size - 1]; }

		char operator[](size_type) const;
		char& operator[](size_type);

		bool operator==(const String&) const;
		bool operator==(StringView) const;
		bool operator==(const char*) const;

		ErrorOr<void> resize(size_type, char = '\0');
		ErrorOr<void> reserve(size_type);
		ErrorOr<void> shrink_to_fit();

		StringView sv() const;

		bool empty() const;
		size_type size() const;
		size_type capacity() const;

		const char* data() const;

	private:
		ErrorOr<void> ensure_capacity(size_type);

		ErrorOr<void> copy_impl(StringView);
		void move_impl(String&&);

	private:
		char*		m_data		= nullptr;
		size_type	m_capacity	= 0;
		size_type	m_size		= 0;	
	};

	template<typename... Args>
	String String::formatted(const char* format, const Args&... args)
	{
		String result;
		BAN::Formatter::print([&](char c){ result.push_back(c); }, format, args...);
		return result;
	}

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
