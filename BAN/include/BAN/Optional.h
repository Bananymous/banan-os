#pragma once

#include <BAN/Assert.h>
#include <BAN/Move.h>
#include <BAN/PlacementNew.h>

#include <stdint.h>

namespace BAN
{

	template<typename T>
	class Optional
	{
	public:
		constexpr Optional();
		constexpr Optional(Optional&&);
		constexpr Optional(const Optional&);
		constexpr Optional(const T&);
		constexpr Optional(T&&);

		~Optional();

		constexpr Optional& operator=(Optional&&);
		constexpr Optional& operator=(const Optional&);

		template<typename... Args>
		constexpr Optional& emplace(Args&&...);

		constexpr T* operator->();
		constexpr const T* operator->() const;

		constexpr T& operator*();
		constexpr const T& operator*() const;

		constexpr bool has_value() const;

		constexpr T release_value();
		constexpr T& value();
		constexpr const T& value() const;
		constexpr T& value_or(T&);
		constexpr const T& value_or(const T&) const;

		constexpr void clear();

	private:
		alignas(T) uint8_t m_storage[sizeof(T)] {};
		bool m_has_value { false };
	};

	template<typename T>
	constexpr Optional<T>::Optional()
		: m_has_value(false)
	{}

	template<typename T>
	constexpr Optional<T>::Optional(Optional<T>&& other)
		: m_has_value(other.has_value())
	{
		if (other.has_value())
			new (m_storage) T(move(other.release_value()));
	}

	template<typename T>
	constexpr Optional<T>::Optional(const Optional<T>& other)
		: m_has_value(other.has_value())
	{
		if (other.has_value())
			new (m_storage) T(other.value());
	}

	template<typename T>
	constexpr Optional<T>::Optional(const T& value)
		: m_has_value(true)
	{
		new (m_storage) T(value);
	}

	template<typename T>
	constexpr Optional<T>::Optional(T&& value)
		: m_has_value(true)
	{
		new (m_storage) T(move(value));
	}

	template<typename T>
	Optional<T>::~Optional()
	{
		clear();
	}

	template<typename T>
	constexpr Optional<T>& Optional<T>::operator=(Optional&& other)
	{
		clear();
		m_has_value = other.has_value();
		if (other.has_value())
			new (m_storage) T(move(other.release_value()));
		return *this;
	}

	template<typename T>
	constexpr Optional<T>& Optional<T>::operator=(const Optional& other)
	{
		clear();
		m_has_value = other.has_value();
		if (other.has_value())
			new (m_storage) T(other.value());
		return *this;
	}

	template<typename T>
	template<typename... Args>
	constexpr Optional<T>& Optional<T>::emplace(Args&&... args)
	{
		clear();
		m_has_value = true;
		new (m_storage) T(forward<Args>(args)...);
		return *this;
	}

	template<typename T>
	constexpr T* Optional<T>::operator->()
	{
		ASSERT(has_value());
		return &value();
	}

	template<typename T>
	constexpr const T* Optional<T>::operator->() const
	{
		ASSERT(has_value());
		return &value();
	}

	template<typename T>
	constexpr T& Optional<T>::operator*()
	{
		ASSERT(has_value());
		return value();
	}

	template<typename T>
	constexpr const T& Optional<T>::operator*() const
	{
		ASSERT(has_value());
		return value();
	}

	template<typename T>
	constexpr bool Optional<T>::has_value() const
	{
		return m_has_value;
	}

	template<typename T>
	constexpr T Optional<T>::release_value()
	{
		ASSERT(has_value());
		T released_value = move(value());
		value().~T();
		m_has_value = false;
		return move(released_value);
	}

	template<typename T>
	constexpr T& Optional<T>::value()
	{
		ASSERT(has_value());
		return *reinterpret_cast<T*>(&m_storage);
	}

	template<typename T>
	constexpr const T& Optional<T>::value() const
	{
		ASSERT(has_value());
		return *reinterpret_cast<const T*>(&m_storage);
	}

	template<typename T>
	constexpr T& Optional<T>::value_or(T& empty)
	{
		if (!has_value())
			return empty;
		return value();
	}

	template<typename T>
	constexpr const T& Optional<T>::value_or(const T& empty) const
	{
		if (!has_value())
			return empty;
		return value();
	}

	template<typename T>
	constexpr void Optional<T>::clear()
	{
		if (m_has_value)
			value().~T();
		m_has_value = false;
	}

}
