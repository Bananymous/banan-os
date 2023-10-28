#pragma once

#include <BAN/Errors.h>
#include <BAN/Formatter.h>
#include <BAN/ForwardList.h>
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
		static constexpr size_type sso_capacity = 15;

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

		void pop_back();
		void remove(size_type);

		void clear();

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

		bool operator==(StringView) const;
		bool operator==(const char*) const;

		ErrorOr<void> resize(size_type, char = '\0');
		ErrorOr<void> reserve(size_type);
		ErrorOr<void> shrink_to_fit();

		StringView sv() const	{ return StringView(data(), size()); }

		bool empty() const		{ return m_size == 0; }
		size_type size() const	{ return m_size; }
		size_type capacity() const;

		char* data();
		const char* data() const;

	private:
		ErrorOr<void> ensure_capacity(size_type);

		bool has_sso() const;

		bool fits_in_sso() const { return fits_in_sso(m_size); }
		static bool fits_in_sso(size_type size) { return size < sso_capacity; }

	private:
		struct SSOStorage
		{
			char storage[sso_capacity + 1] {};
		};
		struct GeneralStorage
		{
			size_type capacity { 0 };
			char* data;
		};

	private:
		Variant<SSOStorage, GeneralStorage>	m_storage	{ SSOStorage() };
		size_type							m_size		{ 0 };
	};

	template<typename... Args>
	String String::formatted(const char* format, const Args&... args)
	{
		String result;
		BAN::Formatter::print([&](char c){ MUST(result.push_back(c)); }, format, args...);
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
