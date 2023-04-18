#pragma once

#include <BAN/Assert.h>
#include <BAN/Math.h>
#include <BAN/Move.h>

namespace BAN
{

	namespace detail
	{

		template<typename T>
		constexpr size_t max_size() { return sizeof(T); }
		template<typename T0, typename T1, typename... Ts>
		constexpr size_t max_size() { return sizeof(T0) > sizeof(T1) ? max_size<T0, Ts...>() : max_size<T1, Ts...>(); }

		template<typename T>
		constexpr size_t max_align() { return alignof(T); }
		template<typename T0, typename T1, typename... Ts>
		constexpr size_t max_align() { return alignof(T0) > alignof(T1) ? max_align<T0, Ts...>() : max_align<T1, Ts...>(); }

		template<typename T, typename T0, typename... Ts>
		constexpr size_t index()
		{
			if constexpr(is_same_v<T, T0>)
				return 0;
			else if constexpr(sizeof...(Ts) == 0)
				return 1;
			else
				return index<T, Ts...>() + 1;
		}

		template<typename T, typename... Ts>
		void destruct(size_t index, uint8_t* data)
		{
			if (index == 0)
				reinterpret_cast<T*>(data)->~T();
			else if constexpr(sizeof...(Ts) > 0)
				destruct<Ts...>(index - 1, data);
			else
				ASSERT_NOT_REACHED();
		}

		template<typename T, typename... Ts>
		void move_construct(size_t index, uint8_t* source, uint8_t* target)
		{
			if (index == 0)
				new (target) T(move(*reinterpret_cast<T*>(source)));
			else if constexpr(sizeof...(Ts) > 0)
				move_construct<Ts...>(index - 1, source, target);
			else
				ASSERT_NOT_REACHED();
		}

		template<typename T, typename... Ts>
		void copy_construct(size_t index, const uint8_t* source, uint8_t* target)
		{
			if (index == 0)
				new (target) T(*reinterpret_cast<const T*>(source));
			else if constexpr(sizeof...(Ts) > 0)
				copy_construct<Ts...>(index - 1, source, target);
			else
				ASSERT_NOT_REACHED();
		}

		template<typename T, typename... Ts>
		void move_assign(size_t index, uint8_t* source, uint8_t* target)
		{
			if (index == 0)
				*reinterpret_cast<T*>(target) = move(*reinterpret_cast<T*>(source));
			else if constexpr(sizeof...(Ts) > 0)
				move_assign<Ts...>(index - 1, source, target);
			else
				ASSERT_NOT_REACHED();
		}

		template<typename T, typename... Ts>
		void copy_assign(size_t index, const uint8_t* source, uint8_t* target)
		{
			if (index == 0)
				*reinterpret_cast<T*>(target) = *reinterpret_cast<const T*>(source);
			else if constexpr(sizeof...(Ts) > 0)
				copy_assign<Ts...>(index - 1, source, target);
			else
				ASSERT_NOT_REACHED();
		}

	}

	template<typename... Ts>
		requires (!is_lvalue_reference_v<Ts> && ...)
	class Variant
	{
	private:
		template<typename T>
		static constexpr bool can_have() { return detail::index<T, Ts...>() != invalid_index(); }
		static constexpr size_t invalid_index() { return sizeof...(Ts); }

	public:
		Variant() = default;

		Variant(Variant&& other)
			: m_index(other.m_index)
		{
			detail::move_construct<Ts...>(other.m_index, other.m_storage, m_storage);
			other.clear();
		}

		Variant(const Variant& other)
			: m_index(other.m_index)
		{
			detail::copy_construct<Ts...>(other.m_index, other.m_storage, m_storage);
		}

		template<typename T>
		Variant(T&& value) requires (can_have<T>())
			: m_index(detail::index<T, Ts...>())
		{
			new (m_storage) T(move(value));	
		}

		template<typename T>
		Variant(const T& value) requires (can_have<T>())
			: m_index(detail::index<T, Ts...>())
		{
			new (m_storage) T(value);	
		}

		~Variant()
		{
			clear();
		}

		Variant& operator=(Variant&& other)
		{
			if (m_index == other.m_index)
			{
				detail::move_assign<Ts...>(m_index, other.m_storage, m_storage);
			}
			else
			{
				clear();
				detail::move_construct<Ts...>(other.m_index, other.m_storage, m_storage);
				m_index = other.m_index;
			}
			other.clear();
			return *this;
		}

		Variant& operator=(const Variant& other)
		{
			if (m_index == other.m_index)
			{
				detail::copy_assign<Ts...>(m_index, other.m_storage, m_storage);
			}
			else
			{
				clear();
				detail::copy_construct<Ts...>(other.m_index, other.m_storage, m_storage);
				m_index = other.m_index;
			}
			return *this;
		}

		template<typename T>
		Variant& operator=(T&& value) requires (can_have<T>())
		{
			*this = Variant(move(value));
			return *this;
		}

		template<typename T>
		Variant& operator=(const T& value) requires (can_have<T>())
		{
			*this = Variant(value);
			return *this;
		}

		template<typename T>
		bool has() const requires (can_have<T>())
		{
			return m_index == detail::index<T, Ts...>();
		}

		template<typename T>
		void set(T&& value) requires (can_have<T>())
		{
			if (has<T>())
				get<T>() = move(value);
			else
			{
				clear();
				m_index = detail::index<T, Ts...>();
				new (m_storage) T(move(value));
			}
		}

		template<typename T>
		void set(const T& value) requires (can_have<T>())
		{
			if (has<T>())
				get<T>() = value;
			else
			{
				clear();
				m_index = detail::index<T, Ts...>();
				new (m_storage) T(value);
			}
		}

		template<typename T>
		T& get() requires (can_have<T>())
		{
			ASSERT(has<T>());
			return *reinterpret_cast<T*>(m_storage);
		}

		template<typename T>
		const T& get() const requires (can_have<T>())
		{
			ASSERT(has<T>());
			return *reinterpret_cast<const T*>(m_storage);
		}

		void clear()
		{
			if (m_index != invalid_index())
			{
				detail::destruct<Ts...>(m_index, m_storage);
				m_index = invalid_index();
			}
		}

	private:
		alignas(detail::max_align<Ts...>()) uint8_t m_storage[detail::max_size<Ts...>()] {};
		size_t m_index { invalid_index() };
	};

}