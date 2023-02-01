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

		[[nodiscard]] ErrorOr<void> push_back(T&&);
		[[nodiscard]] ErrorOr<void> push_back(const T&);
		template<typename... Args>
		[[nodiscard]] ErrorOr<void> emplace_back(Args...);
		template<typename... Args>
		[[nodiscard]] ErrorOr<void> emplace(size_type, Args...);
		[[nodiscard]] ErrorOr<void> insert(size_type, T&&);
		[[nodiscard]] ErrorOr<void> insert(size_type, const T&);
		
		iterator begin();
		iterator end();
		const_iterator begin() const;
		const_iterator end() const;

		void pop_back();
		void remove(size_type);
		void clear();

		bool has(const T&) const;

		const T& operator[](size_type) const;
		T& operator[](size_type);

		const T& back() const;
		T& back();
		const T& front() const;
		T& front();

		[[nodiscard]] ErrorOr<void> resize(size_type);
		[[nodiscard]] ErrorOr<void> reserve(size_type);

		bool empty() const;
		size_type size() const;
		size_type capacity() const;

	private:
		[[nodiscard]] ErrorOr<void> ensure_capacity(size_type);
		const T* address_of(size_type, void* = nullptr) const;
		T* address_of(size_type, void* = nullptr);

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
		MUST(ensure_capacity(other.m_size));
		for (size_type i = 0; i < other.m_size; i++)
			new (address_of(i)) T(other[i]);
		m_size = other.m_size;
	}

	template<typename T>
	Vector<T>::~Vector()
	{
		clear();
	}

	template<typename T>
	Vector<T>& Vector<T>::operator=(Vector<T>&& other)
	{
		clear();

		m_data = other.m_data;
		m_capacity = other.m_capacity;
		m_size = other.m_size;

		other.m_data = nullptr;
		other.m_capacity = 0;
		other.m_size = 0;

		return *this;
	}

	template<typename T>
	Vector<T>& Vector<T>::operator=(const Vector<T>& other)
	{
		clear();
		MUST(ensure_capacity(other.size()));
		for (size_type i = 0; i < other.size(); i++)
			new (address_of(i)) T(other[i]);
		m_size = other.m_size;
		return *this;
	}

	template<typename T>
	ErrorOr<void> Vector<T>::push_back(T&& value)
	{
		TRY(ensure_capacity(m_size + 1));
		new (address_of(m_size)) T(move(value));
		m_size++;
		return {};
	}

	template<typename T>
	ErrorOr<void> Vector<T>::push_back(const T& value)
	{
		return push_back(move(T(value)));
	}

	template<typename T>
	template<typename... Args>
	ErrorOr<void> Vector<T>::emplace_back(Args... args)
	{
		TRY(ensure_capacity(m_size + 1));
		new (address_of(m_size)) T(forward<Args>(args)...);
		m_size++;
		return {};
	}

	template<typename T>
	template<typename... Args>
	ErrorOr<void> Vector<T>::emplace(size_type index, Args... args)
	{
		ASSERT(index <= m_size);
		TRY(ensure_capacity(m_size + 1));
		if (index < m_size)
		{
			new (address_of(m_size)) T(move(*address_of(m_size - 1)));
			for (size_type i = m_size - 1; i > index; i--)
				*address_of(i) = move(*address_of(i - 1));
			*address_of(index) = move(T(forward<Args>(args)...));
		}
		else
		{
			new (address_of(m_size)) T(forward<Args>(args)...);
		}
		m_size++;
		return {};
	}

	template<typename T>
	ErrorOr<void> Vector<T>::insert(size_type index, T&& value)
	{
		ASSERT(index <= m_size);
		TRY(ensure_capacity(m_size + 1));
		if (index < m_size)
		{
			new (address_of(m_size)) T(move(*address_of(m_size - 1)));
			for (size_type i = m_size - 1; i > index; i--)
				*address_of(i) = move(*address_of(i - 1));
			*address_of(index) = move(value);
		}
		else
		{
			new (address_of(m_size)) T(move(value));
		}
		m_size++;
		return {};
	}

	template<typename T>
	ErrorOr<void> Vector<T>::insert(size_type index, const T& value)
	{
		return insert(move(T(value)), index);
	}

	template<typename T>
	typename Vector<T>::iterator Vector<T>::begin()
	{
		return VectorIterator<T>(address_of(0));
	}

	template<typename T>
	typename Vector<T>::iterator Vector<T>::end()
	{
		return VectorIterator<T>(address_of(m_size));
	}

	template<typename T>
	typename Vector<T>::const_iterator Vector<T>::begin() const
	{
		return VectorConstIterator<T>(address_of(0));
	}

	template<typename T>
	typename Vector<T>::const_iterator Vector<T>::end() const
	{
		return VectorConstIterator<T>(address_of(m_size));
	}

	template<typename T>
	void Vector<T>::pop_back()
	{
		ASSERT(m_size > 0);
		address_of(m_size - 1)->~T();
		m_size--;
	}

	template<typename T>
	void Vector<T>::remove(size_type index)
	{
		ASSERT(index < m_size);
		for (size_type i = index; i < m_size - 1; i++)
			*address_of(i) = move(*address_of(i + 1));
		address_of(m_size - 1)->~T();
		m_size--;
	}

	template<typename T>
	void Vector<T>::clear()
	{
		for (size_type i = 0; i < m_size; i++)
			address_of(i)->~T();
		BAN::deallocator(m_data);
		m_data = nullptr;
		m_capacity = 0;
		m_size = 0;
	}

	template<typename T>
	bool Vector<T>::has(const T& other) const
	{
		for (size_type i = 0; i < m_size; i++)
			if (*address_of(i) == other)
				return true;
		return false;
	}
	
	template<typename T>
	const T& Vector<T>::operator[](size_type index) const
	{
		ASSERT(index < m_size);
		return *address_of(index);
	}

	template<typename T>
	T& Vector<T>::operator[](size_type index)
	{
		ASSERT(index < m_size);
		return *address_of(index);
	}

	template<typename T>
	const T& Vector<T>::back() const
	{
		ASSERT(m_size > 0);
		return *address_of(m_size - 1);
	}

	template<typename T>
	T& Vector<T>::back()
	{
		ASSERT(m_size > 0);
		return *address_of(m_size - 1);
	}

	template<typename T>
	const T& Vector<T>::front() const
	{
		ASSERT(m_size > 0);
		return *address_of(0);
	}
	template<typename T>
	T& Vector<T>::front()
	{
		ASSERT(m_size > 0);
		return *address_of(0);
	}

	template<typename T>
	ErrorOr<void> Vector<T>::resize(size_type size)
	{
		TRY(ensure_capacity(size));
		if (size < m_size)
			for (size_type i = size; i < m_size; i++)
				address_of(i)->~T();
		if (size > m_size)
			for (size_type i = m_size; i < size; i++)
				new (address_of(i)) T();
		m_size = size;
		return {};
	}

	template<typename T>
	ErrorOr<void> Vector<T>::reserve(size_type size)
	{
		TRY(ensure_capacity(size));
		return {};
	}

	template<typename T>
	bool Vector<T>::empty() const
	{
		return m_size == 0;
	}

	template<typename T>
	typename Vector<T>::size_type Vector<T>::size() const
	{
		return m_size;
	}

	template<typename T>
	typename Vector<T>::size_type Vector<T>::capacity() const
	{
		return m_capacity;
	}

	template<typename T>
	ErrorOr<void> Vector<T>::ensure_capacity(size_type size)
	{
		if (m_capacity >= size)
			return {};
		size_type new_cap = BAN::Math::max<size_type>(size, m_capacity * 3 / 2);
		uint8_t* new_data = (uint8_t*)BAN::allocator(new_cap * sizeof(T));
		if (new_data == nullptr)
			return Error::from_string("Vector: Could not allocate memory");
		for (size_type i = 0; i < m_size; i++)
		{
			new (address_of(i, new_data)) T(move(*address_of(i)));
			address_of(i)->~T();
		}
		BAN::deallocator(m_data);
		m_data = new_data;
		m_capacity = new_cap;
		return {};
	}

	template<typename T>
	const T* Vector<T>::address_of(size_type index, void* base) const
	{
		if (base == nullptr)
			base = m_data;
		return (T*)base + index;
	}

	template<typename T>
	T* Vector<T>::address_of(size_type index, void* base)
	{
		if (base == nullptr)
			base = m_data;
		return (T*)base + index;
	}

}