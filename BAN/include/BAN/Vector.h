#pragma once

#include <BAN/Errors.h>
#include <BAN/Math.h>
#include <BAN/Memory.h>

#include <string.h>

namespace BAN
{

	template<typename T>
	class Vector
	{
	public:
		using size_type = size_t;
		using value_type = T;

	public:
		Vector() = default;
		Vector(const Vector<T>&);
		~Vector();

		ErrorOr<void> PushBack(const T&);
		ErrorOr<void> Insert(const T&, size_type);
		
		void PopBack();
		void Remove(size_type);

		bool Has(const T&) const;

		const T& operator[](size_type) const;
		T& operator[](size_type);

		const T& Back() const;
		T& Back();
		const T& Front() const;
		T& Front();

		ErrorOr<void> Resize(size_type);
		ErrorOr<void> Reserve(size_type);

		bool Empty() const;
		size_type Size() const;
		size_type Capasity() const;

	private:
		ErrorOr<void> EnsureCapasity(size_type);

	private:
		T*			m_data		= nullptr;
		size_type	m_capasity	= 0;
		size_type	m_size		= 0;	
	};

	template<typename T>
	Vector<T>::Vector(const Vector<T>& other)
	{
		MUST(EnsureCapasity(other.m_size));
		for (size_type i = 0; i < other.m_size; i++)
			m_data[i] = other[i];
		m_size = other.m_size;
	}

	template<typename T>
	Vector<T>::~Vector()
	{
		for (size_type i = 0; i < m_size; i++)
			m_data[i].~T();
		BAN::deallocator(m_data);
	}

	template<typename T>
	ErrorOr<void> Vector<T>::PushBack(const T& value)
	{
		TRY(EnsureCapasity(m_size + 1));
		m_data[m_size] = value;
		m_size++;
		return {};
	}

	template<typename T>
	ErrorOr<void> Vector<T>::Insert(const T& value, size_type index)
	{
		ASSERT(index <= m_size);
		TRY(EnsureCapasity(m_size + 1));
		memmove(m_data + index + 1, m_data + index, (m_size - index) * sizeof(T));
		m_data[index] = value;
		m_size++;
		return {};
	}

	template<typename T>
	void Vector<T>::PopBack()
	{
		ASSERT(m_size > 0);
		m_data[m_size - 1].~T();
		m_size--;
	}

	template<typename T>
	void Vector<T>::Remove(size_type index)
	{
		ASSERT(index < m_size);
		m_data[index].~T();
		memmove(m_data + index, m_data + index + 1, (m_size - index - 1) * sizeof(T));
		m_size--;
	}

	template<typename T>
	bool Vector<T>::Has(const T& other) const
	{
		for (size_type i = 0; i < m_size; i++)
			if (m_data[i] == other)
				return true;
		return false;
	}
	
	template<typename T>
	const T& Vector<T>::operator[](size_type index) const
	{
		ASSERT(index < m_size);
		return m_data[index];
	}

	template<typename T>
	T& Vector<T>::operator[](size_type index)
	{
		ASSERT(index < m_size);
		return m_data[index];
	}

	template<typename T>
	const T& Vector<T>::Back() const
	{
		ASSERT(m_size > 0);
		return m_data[m_size - 1];
	}

	template<typename T>
	T& Vector<T>::Back()
	{
		ASSERT(m_size > 0);
		return m_data[m_size - 1];
	}

	template<typename T>
	const T& Vector<T>::Front() const
	{
		ASSERT(m_size > 0);
		return m_data[0];
	}
	template<typename T>
	T& Vector<T>::Front()
	{
		ASSERT(m_size > 0);
		return m_data[0];
	}

	template<typename T>
	ErrorOr<void> Vector<T>::Resize(size_type size)
	{
		if (size < m_size)
		{
			for (size_type i = size; i < m_size; i++)
				m_data[i].~T();
			m_size = size;
		}
		else if (size > m_size)
		{
			TRY(EnsureCapasity(size));
			for (size_type i = m_size; i < size; i++)
				m_data[i] = T();
			m_size = size;
		}
		m_size = size;
		return {};
	}

	template<typename T>
	ErrorOr<void> Vector<T>::Reserve(size_type size)
	{
		TRY(EnsureCapasity(size));
		return {};
	}

	template<typename T>
	bool Vector<T>::Empty() const
	{
		return m_size == 0;
	}

	template<typename T>
	typename Vector<T>::size_type Vector<T>::Size() const
	{
		return m_size;
	}

	template<typename T>
	typename Vector<T>::size_type Vector<T>::Capasity() const
	{
		return m_capasity;
	}

	template<typename T>
	ErrorOr<void> Vector<T>::EnsureCapasity(size_type size)
	{
		if (m_capasity >= size)
			return {};
		size_type new_cap = BAN::Math::max<size_type>(size, m_capasity * 3 / 2);
		void* new_data = BAN::allocator(new_cap * sizeof(T));
		if (new_data == nullptr)
			return Error::FromString("Vector: Could not allocate memory");
		memcpy(new_data, m_data, m_size * sizeof(T));
		BAN::deallocator(m_data);
		m_data = (T*)new_data;
		m_capasity = new_cap;
		return {};
	}

}