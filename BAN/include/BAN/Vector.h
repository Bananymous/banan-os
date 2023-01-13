#pragma once

#include <BAN/Errors.h>
#include <BAN/Math.h>
#include <BAN/Move.h>

namespace BAN
{

	// T must be move assignable, move constructable (and copy constructable for some functions)
	template<typename T>
	class Vector
	{
	public:
		using size_type = size_t;
		using value_type = T;

	public:
		Vector() = default;
		Vector(Vector<T>&&);
		Vector(const Vector<T>&);
		~Vector();

		Vector<T>& operator=(Vector<T>&&);
		Vector<T>& operator=(const Vector<T>&);

		[[nodiscard]] ErrorOr<void> PushBack(T&&);
		[[nodiscard]] ErrorOr<void> PushBack(const T&);
		[[nodiscard]] ErrorOr<void> Insert(T&&, size_type);
		[[nodiscard]] ErrorOr<void> Insert(const T&, size_type);
		
		void PopBack();
		void Remove(size_type);
		void Clear();

		bool Has(const T&) const;

		const T& operator[](size_type) const;
		T& operator[](size_type);

		const T& Back() const;
		T& Back();
		const T& Front() const;
		T& Front();

		[[nodiscard]] ErrorOr<void> Resize(size_type);
		[[nodiscard]] ErrorOr<void> Reserve(size_type);

		bool Empty() const;
		size_type Size() const;
		size_type Capacity() const;

	private:
		T* Address(size_type, uint8_t* = nullptr) const;
		[[nodiscard]] ErrorOr<void> EnsureCapasity(size_type);

	private:
		uint8_t*	m_data		= nullptr;
		size_type	m_capacity	= 0;
		size_type	m_size		= 0;	
	};

	template<typename T>
	Vector<T>::Vector(Vector<T>&& other)
	{
		m_data = other.m_data;
		m_capacity = other.m_capacity;
		m_size = other.m_size;

		other.m_data = nullptr;
		other.m_capacity = 0;
		other.m_size = 0;
	}

	template<typename T>
	Vector<T>::Vector(const Vector<T>& other)
	{
		MUST(EnsureCapasity(other.m_size));
		for (size_type i = 0; i < other.m_size; i++)
			new (Address(i)) T(other[i]);
		m_size = other.m_size;
	}

	template<typename T>
	Vector<T>::~Vector()
	{
		Clear();
	}

	template<typename T>
	Vector<T>& Vector<T>::operator=(Vector<T>&& other)
	{
		Clear();

		m_data = other.m_data;
		m_capacity = other.m_capacity;
		m_size = other.m_size;

		other.m_data = nullptr;
		other.m_capacity = 0;
		other.m_size = 0;
	}

	template<typename T>
	Vector<T>& Vector<T>::operator=(const Vector<T>& other)
	{
		Clear();
		MUST(EnsureCapasity(other.Size()));
		for (size_type i = 0; i < other.Size(); i++)
			new (Address(i)) T(other[i]);
		m_size = other.m_size;
		return *this;
	}

	template<typename T>
	ErrorOr<void> Vector<T>::PushBack(T&& value)
	{
		TRY(EnsureCapasity(m_size + 1));
		new (Address(m_size)) T(Move(value));
		m_size++;
		return {};
	}

	template<typename T>
	ErrorOr<void> Vector<T>::PushBack(const T& value)
	{
		return PushBack(Move(T(value)));
	}

	template<typename T>
	ErrorOr<void> Vector<T>::Insert(T&& value, size_type index)
	{
		ASSERT(index <= m_size);
		TRY(EnsureCapasity(m_size + 1));
		if (index < m_size)
		{
			new (Address(m_size)) T(Move(*Address(m_size - 1)));
			for (size_type i = m_size - 1; i > index; i--)
				*Address(i) = Move(*Address(i - 1));
		}
		*Address(index) = Move(value);
		m_size++;
		return {};
	}

	template<typename T>
	ErrorOr<void> Vector<T>::Insert(const T& value, size_type index)
	{
		return Insert(Move(T(value)), index);
	}

	template<typename T>
	void Vector<T>::PopBack()
	{
		ASSERT(m_size > 0);
		Address(m_size - 1)->~T();
		m_size--;
	}

	template<typename T>
	void Vector<T>::Remove(size_type index)
	{
		ASSERT(index < m_size);
		for (size_type i = index; i < m_size - 1; i++)
			*Address(i) = Move(*Address(i + 1));
		Address(m_size - 1)->~T();
		m_size--;
	}

	template<typename T>
	void Vector<T>::Clear()
	{
		for (size_type i = 0; i < m_size; i++)
			Address(i)->~T();
		BAN::deallocator(m_data);
		m_data = nullptr;
		m_capacity = 0;
		m_size = 0;
	}

	template<typename T>
	bool Vector<T>::Has(const T& other) const
	{
		for (size_type i = 0; i < m_size; i++)
			if (*Address(i) == other)
				return true;
		return false;
	}
	
	template<typename T>
	const T& Vector<T>::operator[](size_type index) const
	{
		ASSERT(index < m_size);
		return *Address(index);
	}

	template<typename T>
	T& Vector<T>::operator[](size_type index)
	{
		ASSERT(index < m_size);
		return *Address(index);
	}

	template<typename T>
	const T& Vector<T>::Back() const
	{
		ASSERT(m_size > 0);
		return *Address(m_size - 1);
	}

	template<typename T>
	T& Vector<T>::Back()
	{
		ASSERT(m_size > 0);
		return *Address(m_size - 1);
	}

	template<typename T>
	const T& Vector<T>::Front() const
	{
		ASSERT(m_size > 0);
		return *Address(0);
	}
	template<typename T>
	T& Vector<T>::Front()
	{
		ASSERT(m_size > 0);
		return *Address(0);
	}

	template<typename T>
	ErrorOr<void> Vector<T>::Resize(size_type size)
	{
		TRY(EnsureCapasity(size));
		if (size < m_size)
			for (size_type i = size; i < m_size; i++)
				Address(i)->~T();
		if (size > m_size)
			for (size_type i = m_size; i < size; i++)
				new (Address(i)) T();
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
	typename Vector<T>::size_type Vector<T>::Capacity() const
	{
		return m_capacity;
	}

	template<typename T>
	ErrorOr<void> Vector<T>::EnsureCapasity(size_type size)
	{
		if (m_capacity >= size)
			return {};
		size_type new_cap = BAN::Math::max<size_type>(size, m_capacity * 3 / 2);
		uint8_t* new_data = (uint8_t*)BAN::allocator(new_cap * sizeof(T));
		if (new_data == nullptr)
			return Error::FromString("Vector: Could not allocate memory");
		for (size_type i = 0; i < m_size; i++)
		{
			new (Address(i, new_data)) T(Move(*Address(i)));
			Address(i)->~T();
		}
		BAN::deallocator(m_data);
		m_data = new_data;
		m_capacity = new_cap;
		return {};
	}

	template<typename T>
	T* Vector<T>::Address(size_type index, uint8_t* base) const
	{
		if (base == nullptr)
			base = m_data;
		return (T*)(base + index * sizeof(T));
	}

}