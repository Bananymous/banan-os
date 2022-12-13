#pragma once

#include <BAN/Errors.h>

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

}