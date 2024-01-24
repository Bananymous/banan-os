#pragma once

#include <BAN/Assert.h>
#include <BAN/Move.h>

#include <stdint.h>

namespace BAN
{

	template<typename T>
	class Optional
	{
	public:
		Optional();
		Optional(Optional&&);
		Optional(const Optional&);
		Optional(const T&);
		Optional(T&&);
		template<typename... Args>
		Optional(Args&&...);

		~Optional();

		Optional& operator=(Optional&&);
		Optional& operator=(const Optional&);

		template<typename... Args>
		Optional& emplace(Args&&...);

		T* operator->();
		const T* operator->() const;

		T& operator*();
		const T& operator*() const;

		bool has_value() const;

		T release_value();
		T& value();
		const T& value() const;

		void clear();

	private:
		alignas(T) uint8_t m_storage[sizeof(T)];
		bool m_has_value { false };
	};

	template<typename T>
	Optional<T>::Optional()
		: m_has_value(false)
	{}

	template<typename T>
	Optional<T>::Optional(Optional<T>&& other)
		: m_has_value(other.has_value())
	{
		if (other.has_value())
			new (m_storage) T(move(other.release_value()));
	}

	template<typename T>
	Optional<T>::Optional(const Optional<T>& other)
		: m_has_value(other.has_value())
	{
		if (other.has_value())
			new (m_storage) T(other.value());
	}

	template<typename T>
	Optional<T>::Optional(const T& value)
		: m_has_value(true)
	{
		new (m_storage) T(value);
	}

	template<typename T>
	Optional<T>::Optional(T&& value)
		: m_has_value(true)
	{
		new (m_storage) T(move(value));
	}

	template<typename T>
	template<typename... Args>
	Optional<T>::Optional(Args&&... args)
		: m_has_value(true)
	{
		new (m_storage) T(forward<Args>(args)...);
	}

	template<typename T>
	Optional<T>::~Optional()
	{
		clear();
	}

	template<typename T>
	Optional<T>& Optional<T>::operator=(Optional&& other)
	{
		clear();
		m_has_value = other.has_value();
		if (other.has_value())
			new (m_storage) T(move(other.release_value()));
		return *this;
	}

	template<typename T>
	Optional<T>& Optional<T>::operator=(const Optional& other)
	{
		clear();
		m_has_value = other.has_value();
		if (other.has_value)
			new (m_storage) T(other.value());
		return *this;
	}

	template<typename T>
	template<typename... Args>
	Optional<T>& Optional<T>::emplace(Args&&... args)
	{
		clear();
		m_has_value = true;
		new (m_storage) T(forward<Args>(args)...);
		return *this;
	}

	template<typename T>
	T* Optional<T>::operator->()
	{
		ASSERT(has_value());
		return &value();
	}

	template<typename T>
	const T* Optional<T>::operator->() const
	{
		ASSERT(has_value());
		return &value();
	}

	template<typename T>
	T& Optional<T>::operator*()
	{
		ASSERT(has_value());
		return value();
	}

	template<typename T>
	const T& Optional<T>::operator*() const
	{
		ASSERT(has_value());
		return value();
	}

	template<typename T>
	bool Optional<T>::has_value() const
	{
		return m_has_value;
	}

	template<typename T>
	T Optional<T>::release_value()
	{
		ASSERT(has_value());
		T released_value = move(value());
		value().~T();
		m_has_value = false;
		return move(released_value);
	}

	template<typename T>
	T& Optional<T>::value()
	{
		ASSERT(has_value());
		return (T&)m_storage;
	}

	template<typename T>
	const T& Optional<T>::value() const
	{
		ASSERT(has_value());
		return (const T&)m_storage;
	}

	template<typename T>
	void Optional<T>::clear()
	{
		if (m_has_value)
			value().~T();
		m_has_value = false;
	}

}
