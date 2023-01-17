#pragma once

#include <BAN/Errors.h>
#include <BAN/Math.h>
#include <BAN/Memory.h>
#include <BAN/Move.h>

namespace BAN
{

	template<typename T>
	class Vector;

	template<typename T>
	class VectorIterator
	{
	public:
		VectorIterator() = default;
		VectorIterator(const VectorIterator& other) : m_data(other.m_data) { }
		VectorIterator& operator=(const VectorIterator& other) { m_data = other.m_data; return *this; }
		VectorIterator& operator++() { m_data++; return *this; }
		T& operator*() { return *m_data; }
		const T& operator*() const { return *m_data; }
		T* operator->() { return m_data; }
		const T* operator->() const { return m_data; }
		bool operator==(const VectorIterator<T>& other) const { return !(*this != other); }
		bool operator!=(const VectorIterator<T>& other) const { return m_data != other.m_data; }
	private:
		VectorIterator(T* data) : m_data(data) { }
	private:
		T* m_data = nullptr;
		friend class Vector<T>;
	};

	template<typename T>
	class VectorConstIterator
	{
	public:
		VectorConstIterator() = default;
		VectorConstIterator(const VectorConstIterator& other) : m_data(other.m_data) { }
		VectorConstIterator& operator=(const VectorConstIterator& other) { m_data = other.m_data; return *this; }
		VectorConstIterator& operator++() { m_data++; return *this; }
		const T& operator*() const { return *m_data; }
		const T* operator->() const { return m_data; }
		bool operator==(const VectorConstIterator<T>& other) const { return !(*this != other); }
		bool operator!=(const VectorConstIterator<T>& other) const { return m_data != other.m_data; }
	private:
		VectorConstIterator(T* data) : m_data(data) { }
	private:
		const T* m_data = nullptr;
		friend class Vector<T>;
	};

	// T must be move assignable, move constructable (and copy constructable for some functions)
	template<typename T>
	class Vector
	{
	public:
		using size_type = size_t;
		using value_type = T;
		using iterator = VectorIterator<T>;
		using const_iterator = VectorConstIterator<T>;

	public:
		Vector() = default;
		Vector(Vector<T>&&);
		Vector(const Vector<T>&);
		~Vector();

		Vector<T>& operator=(Vector<T>&&);
		Vector<T>& operator=(const Vector<T>&);

		[[nodiscard]] ErrorOr<void> PushBack(T&&);
		[[nodiscard]] ErrorOr<void> PushBack(const T&);
		template<typename... Args>
		[[nodiscard]] ErrorOr<void> EmplaceBack(Args...);
		template<typename... Args>
		[[nodiscard]] ErrorOr<void> Emplace(size_type, Args...);
		[[nodiscard]] ErrorOr<void> Insert(size_type, T&&);
		[[nodiscard]] ErrorOr<void> Insert(size_type, const T&);
		
		iterator begin();
		iterator end();
		const_iterator begin() const;
		const_iterator end() const;

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
		const T* AddressOf(size_type, uint8_t* = nullptr) const;
		T* AddressOf(size_type, uint8_t* = nullptr);
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
			new (AddressOf(i)) T(other[i]);
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
			new (AddressOf(i)) T(other[i]);
		m_size = other.m_size;
		return *this;
	}

	template<typename T>
	ErrorOr<void> Vector<T>::PushBack(T&& value)
	{
		TRY(EnsureCapasity(m_size + 1));
		new (AddressOf(m_size)) T(Move(value));
		m_size++;
		return {};
	}

	template<typename T>
	ErrorOr<void> Vector<T>::PushBack(const T& value)
	{
		return PushBack(Move(T(value)));
	}

	template<typename T>
	template<typename... Args>
	ErrorOr<void> Vector<T>::EmplaceBack(Args... args)
	{
		TRY(EnsureCapasity(m_size + 1));
		new (AddressOf(m_size)) T(Forward<Args>(args)...);
		m_size++;
		return {};
	}

	template<typename T>
	template<typename... Args>
	ErrorOr<void> Vector<T>::Emplace(size_type index, Args... args)
	{
		ASSERT(index <= m_size);
		TRY(EnsureCapasity(m_size + 1));
		if (index < m_size)
		{
			new (AddressOf(m_size)) T(Move(*AddressOf(m_size - 1)));
			for (size_type i = m_size - 1; i > index; i--)
				*AddressOf(i) = Move(*AddressOf(i - 1));
			*AddressOf(index) = Move(T(Forward<Args>(args)...));
		}
		else
		{
			new (AddressOf(m_size)) T(Forward<Args>(args)...);
		}
		m_size++;
		return {};
	}

	template<typename T>
	ErrorOr<void> Vector<T>::Insert(size_type index, T&& value)
	{
		ASSERT(index <= m_size);
		TRY(EnsureCapasity(m_size + 1));
		if (index < m_size)
		{
			new (AddressOf(m_size)) T(Move(*AddressOf(m_size - 1)));
			for (size_type i = m_size - 1; i > index; i--)
				*AddressOf(i) = Move(*AddressOf(i - 1));
			*AddressOf(index) = Move(value);
		}
		else
		{
			new (AddressOf(m_size)) T(Move(value));
		}
		m_size++;
		return {};
	}

	template<typename T>
	ErrorOr<void> Vector<T>::Insert(size_type index, const T& value)
	{
		return Insert(Move(T(value)), index);
	}

	template<typename T>
	typename Vector<T>::iterator Vector<T>::begin()
	{
		return VectorIterator<T>(AddressOf(0));
	}

	template<typename T>
	typename Vector<T>::iterator Vector<T>::end()
	{
		return VectorIterator<T>(AddressOf(m_size));
	}

	template<typename T>
	typename Vector<T>::const_iterator Vector<T>::begin() const
	{
		return VectorConstIterator<T>(AddressOf(0));
	}

	template<typename T>
	typename Vector<T>::const_iterator Vector<T>::end() const
	{
		return VectorConstIterator<T>(AddressOf(m_size));
	}

	template<typename T>
	void Vector<T>::PopBack()
	{
		ASSERT(m_size > 0);
		AddressOf(m_size - 1)->~T();
		m_size--;
	}

	template<typename T>
	void Vector<T>::Remove(size_type index)
	{
		ASSERT(index < m_size);
		for (size_type i = index; i < m_size - 1; i++)
			*AddressOf(i) = Move(*AddressOf(i + 1));
		AddressOf(m_size - 1)->~T();
		m_size--;
	}

	template<typename T>
	void Vector<T>::Clear()
	{
		for (size_type i = 0; i < m_size; i++)
			AddressOf(i)->~T();
		BAN::deallocator(m_data);
		m_data = nullptr;
		m_capacity = 0;
		m_size = 0;
	}

	template<typename T>
	bool Vector<T>::Has(const T& other) const
	{
		for (size_type i = 0; i < m_size; i++)
			if (*AddressOf(i) == other)
				return true;
		return false;
	}
	
	template<typename T>
	const T& Vector<T>::operator[](size_type index) const
	{
		ASSERT(index < m_size);
		return *AddressOf(index);
	}

	template<typename T>
	T& Vector<T>::operator[](size_type index)
	{
		ASSERT(index < m_size);
		return *AddressOf(index);
	}

	template<typename T>
	const T& Vector<T>::Back() const
	{
		ASSERT(m_size > 0);
		return *AddressOf(m_size - 1);
	}

	template<typename T>
	T& Vector<T>::Back()
	{
		ASSERT(m_size > 0);
		return *AddressOf(m_size - 1);
	}

	template<typename T>
	const T& Vector<T>::Front() const
	{
		ASSERT(m_size > 0);
		return *AddressOf(0);
	}
	template<typename T>
	T& Vector<T>::Front()
	{
		ASSERT(m_size > 0);
		return *AddressOf(0);
	}

	template<typename T>
	ErrorOr<void> Vector<T>::Resize(size_type size)
	{
		TRY(EnsureCapasity(size));
		if (size < m_size)
			for (size_type i = size; i < m_size; i++)
				AddressOf(i)->~T();
		if (size > m_size)
			for (size_type i = m_size; i < size; i++)
				new (AddressOf(i)) T();
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
			new (AddressOf(i, new_data)) T(Move(*AddressOf(i)));
			AddressOf(i)->~T();
		}
		BAN::deallocator(m_data);
		m_data = new_data;
		m_capacity = new_cap;
		return {};
	}

	template<typename T>
	const T* Vector<T>::AddressOf(size_type index, uint8_t* base) const
	{
		if (base == nullptr)
			base = m_data;
		return (T*)(base + index * sizeof(T));
	}

	template<typename T>
	T* Vector<T>::AddressOf(size_type index, uint8_t* base)
	{
		if (base == nullptr)
			base = m_data;
		return (T*)(base + index * sizeof(T));
	}

}