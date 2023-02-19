#pragma once

#include <BAN/Errors.h>
#include <BAN/Math.h>
#include <BAN/Memory.h>
#include <BAN/Move.h>

namespace BAN
{

	template<typename T, bool CONST>
	class VectorIterator;

	// T must be move assignable, move constructable (and copy constructable for some functions)
	template<typename T>
	class Vector
	{
	public:
		using size_type = size_t;
		using value_type = T;
		using iterator = VectorIterator<T, false>;
		using const_iterator = VectorIterator<T, true>;

	public:
		Vector() = default;
		Vector(Vector<T>&&);
		Vector(const Vector<T>&);
		Vector(size_type, const T& = T());
		~Vector();

		Vector<T>& operator=(Vector<T>&&);
		Vector<T>& operator=(const Vector<T>&);

		ErrorOr<void> push_back(T&&);
		ErrorOr<void> push_back(const T&);
		template<typename... Args>
		ErrorOr<void> emplace_back(Args&&...);
		template<typename... Args>
		ErrorOr<void> emplace(size_type, Args&&...);
		ErrorOr<void> insert(size_type, T&&);
		ErrorOr<void> insert(size_type, const T&);
		
		iterator begin()				{ return iterator      (m_data); }
		const_iterator begin() const	{ return const_iterator(m_data); }
		iterator end()					{ return iterator      (m_data + m_size); }
		const_iterator end() const		{ return const_iterator(m_data + m_size); }

		void pop_back();
		void remove(size_type);
		void clear();

		T* data() { return m_data; }
		const T* data() const { return m_data; }

		bool contains(const T&) const;

		const T& operator[](size_type) const;
		T& operator[](size_type);

		const T& back() const;
		T& back();
		const T& front() const;
		T& front();

		ErrorOr<void> resize(size_type);
		ErrorOr<void> reserve(size_type);

		bool empty() const;
		size_type size() const;
		size_type capacity() const;

	private:
		ErrorOr<void> ensure_capacity(size_type);

	private:
		T*			m_data		= nullptr;
		size_type	m_capacity	= 0;
		size_type	m_size		= 0;	
	};

	template<typename T, bool CONST>
	class VectorIterator
	{
	public:
		using value_type = T;
		using data_type = maybe_const_t<CONST, T>;

	public:
		VectorIterator() = default;
		template<bool C>
		VectorIterator(const VectorIterator<T, C>& other, enable_if_t<C == CONST || !C>)
			: m_data(other.m_data)
		{
		}
		
		VectorIterator<T, CONST>& operator++()		{ m_data++; return *this; }
		VectorIterator<T, CONST>& operator--()		{ m_data--; return *this; }
		VectorIterator<T, CONST> operator++(int)	{ auto temp = *this; ++(*this); return temp; }
		VectorIterator<T, CONST> operator--(int)	{ auto temp = *this; --(*this); return temp; }

		template<bool ENABLE = !CONST>
		enable_if_t<ENABLE, T&> operator*()		{ ASSERT(m_data); return *m_data; }
		const T& operator*() const				{ ASSERT(m_data); return *m_data; }

		template<bool ENABLE = !CONST>
		enable_if_t<ENABLE, T*> operator->()	{ ASSERT(m_data); return m_data; }
		const T* operator->() const				{ ASSERT(m_data); return m_data; }

		bool operator==(const VectorIterator<T, CONST>& other) const { return m_data == other.m_data; }
		bool operator!=(const VectorIterator<T, CONST>& other) const { return !(*this == other); }

		operator bool() const { return m_data; }

	private:
		VectorIterator(data_type* data) : m_data(data) { }

	private:
		data_type* m_data = nullptr;

		friend class Vector<T>;
		friend class VectorIterator<T, !CONST>;
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
			new (m_data + i) T(other.m_data[i]);
		m_size = other.m_size;
	}

	template<typename T>
	Vector<T>::Vector(size_type size, const T& value)
	{
		MUST(ensure_capacity(size));
		for (size_type i = 0; i < size; i++)
			new (m_data + i) T(value);
		m_size = size;
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
			new (m_data + i) T(other[i]);
		m_size = other.m_size;
		return *this;
	}

	template<typename T>
	ErrorOr<void> Vector<T>::push_back(T&& value)
	{
		TRY(ensure_capacity(m_size + 1));
		new (m_data + m_size) T(move(value));
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
	ErrorOr<void> Vector<T>::emplace_back(Args&&... args)
	{
		TRY(ensure_capacity(m_size + 1));
		new (m_data + m_size) T(forward<Args>(args)...);
		m_size++;
		return {};
	}

	template<typename T>
	template<typename... Args>
	ErrorOr<void> Vector<T>::emplace(size_type index, Args&&... args)
	{
		ASSERT(index <= m_size);
		TRY(ensure_capacity(m_size + 1));
		if (index < m_size)
		{
			new (m_data + m_size) T(move(m_data[m_size - 1]));
			for (size_type i = m_size - 1; i > index; i--)
				m_data[i] = move(m_data[i - 1]);
			m_data[index] = move(T(forward<Args>(args)...));
		}
		else
		{
			new (m_data + m_size) T(forward<Args>(args)...);
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
			new (m_data + m_size) T(move(m_data[m_size - 1]));
			for (size_type i = m_size - 1; i > index; i--)
				m_data[i] = move(m_data[i - 1]);
			m_data[index] = move(value);
		}
		else
		{
			new (m_data + m_size) T(move(value));
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
	void Vector<T>::pop_back()
	{
		ASSERT(m_size > 0);
		m_data[m_size - 1].~T();
		m_size--;
	}

	template<typename T>
	void Vector<T>::remove(size_type index)
	{
		ASSERT(index < m_size);
		for (size_type i = index; i < m_size - 1; i++)
			m_data[i] = move(m_data[i + 1]);
		m_data[m_size - 1].~T();
		m_size--;
	}

	template<typename T>
	void Vector<T>::clear()
	{
		for (size_type i = 0; i < m_size; i++)
			m_data[i].~T();
		BAN::deallocator(m_data);
		m_data = nullptr;
		m_capacity = 0;
		m_size = 0;
	}

	template<typename T>
	bool Vector<T>::contains(const T& other) const
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
	const T& Vector<T>::back() const
	{
		ASSERT(m_size > 0);
		return m_data[m_size - 1];
	}

	template<typename T>
	T& Vector<T>::back()
	{
		ASSERT(m_size > 0);
		return m_data[m_size - 1];
	}

	template<typename T>
	const T& Vector<T>::front() const
	{
		ASSERT(m_size > 0);
		return m_data[0];
	}
	template<typename T>
	T& Vector<T>::front()
	{
		ASSERT(m_size > 0);
		return m_data[0];
	}

	template<typename T>
	ErrorOr<void> Vector<T>::resize(size_type size)
	{
		TRY(ensure_capacity(size));
		if (size < m_size)
			for (size_type i = size; i < m_size; i++)
				m_data[i].~T();
		if (size > m_size)
			for (size_type i = m_size; i < size; i++)
				new (m_data + i) T();
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
		T* new_data = (T*)BAN::allocator(new_cap * sizeof(T));
		if (new_data == nullptr)
			return Error::from_string("Vector: Could not allocate memory");
		for (size_type i = 0; i < m_size; i++)
		{
			new (new_data + i) T(move(m_data[i]));
			m_data[i].~T();
		}
		BAN::deallocator(m_data);
		m_data = new_data;
		m_capacity = new_cap;
		return {};
	}

}

namespace BAN::Formatter
{

	template<typename F, typename T>
	void print_argument_impl(F putc, const Vector<T>& vector, const ValueFormat& format)
	{
		putc('[');
		for (typename Vector<T>::size_type i = 0; i < vector.size(); i++)
		{
			if (i != 0) putc(',');
			print_argument_impl(putc, vector[i], format);
		}
		putc(']');
	}

}
