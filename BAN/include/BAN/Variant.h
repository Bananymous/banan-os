#pragma once

#include <BAN/Assert.h>
#include <BAN/Math.h>
#include <BAN/Move.h>

#include <string.h>

namespace BAN
{

	namespace detail
	{

		template<typename T>
		constexpr size_t size_ref_as_ptr() { return is_lvalue_reference_v<T> ? sizeof(remove_reference_t<T>*) : sizeof(T); }
		template<typename T>
		constexpr size_t align_ref_as_ptr() { return is_lvalue_reference_v<T> ? alignof(remove_reference_t<T>*) : alignof(T); }

		template<typename T>
		constexpr size_t max_size_ref_as_ptr() { return size_ref_as_ptr<T>(); }
		template<typename T0, typename T1, typename... Ts>
		constexpr size_t max_size_ref_as_ptr() { return size_ref_as_ptr<T0>() > size_ref_as_ptr<T1>() ? max_size_ref_as_ptr<T0, Ts...>() : max_size_ref_as_ptr<T1, Ts...>(); }

		template<typename T>
		constexpr size_t max_align_ref_as_ptr() { return align_ref_as_ptr<T>(); }
		template<typename T0, typename T1, typename... Ts>
		constexpr size_t max_align_ref_as_ptr() { return align_ref_as_ptr<T0>() > align_ref_as_ptr<T1>() ? max_align_ref_as_ptr<T0, Ts...>() : max_align_ref_as_ptr<T1, Ts...>(); }

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
				if constexpr(!is_lvalue_reference_v<T>)
					reinterpret_cast<T*>(data)->~T();
				else;
			else if constexpr(sizeof...(Ts) > 0)
				destruct<Ts...>(index - 1, data);
			else
				ASSERT_NOT_REACHED();
		}

		template<typename T, typename... Ts>
		void move_construct(size_t index, uint8_t* source, uint8_t* target)
		{
			if (index == 0)
				if constexpr(!is_lvalue_reference_v<T>)
					new (target) T(move(*reinterpret_cast<T*>(source)));
				else
					memcpy(target, source, sizeof(remove_reference_t<T>*));
			else if constexpr(sizeof...(Ts) > 0)
				move_construct<Ts...>(index - 1, source, target);
			else
				ASSERT_NOT_REACHED();
		}

		template<typename T, typename... Ts>
		void copy_construct(size_t index, const uint8_t* source, uint8_t* target)
		{
			if (index == 0)
				if constexpr(!is_lvalue_reference_v<T>)
					new (target) T(*reinterpret_cast<const T*>(source));
				else
					memcpy(target, source, sizeof(remove_reference_t<T>*));
			else if constexpr(sizeof...(Ts) > 0)
				copy_construct<Ts...>(index - 1, source, target);
			else
				ASSERT_NOT_REACHED();
		}

		template<typename T, typename... Ts>
		void move_assign(size_t index, uint8_t* source, uint8_t* target)
		{
			if (index == 0)
				if constexpr(!is_lvalue_reference_v<T>)
					*reinterpret_cast<T*>(target) = move(*reinterpret_cast<T*>(source));
				else
					memcpy(target, source, sizeof(remove_reference_t<T>*));
			else if constexpr(sizeof...(Ts) > 0)
				move_assign<Ts...>(index - 1, source, target);
			else
				ASSERT_NOT_REACHED();
		}

		template<typename T, typename... Ts>
		void copy_assign(size_t index, const uint8_t* source, uint8_t* target)
		{
			if (index == 0)
				if constexpr(!is_lvalue_reference_v<T>)
					*reinterpret_cast<T*>(target) = *reinterpret_cast<const T*>(source);
				else
					memcpy(target, source, sizeof(remove_reference_t<T>*));
			else if constexpr(sizeof...(Ts) > 0)
				copy_assign<Ts...>(index - 1, source, target);
			else
				ASSERT_NOT_REACHED();
		}

	}

	template<typename... Ts>
		requires (!is_const_v<Ts> && ...)
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
		Variant(T&& value) requires (can_have<T>() && !is_lvalue_reference_v<T>)
			: m_index(detail::index<T, Ts...>())
		{
			new (m_storage) T(move(value));	
		}

		template<typename T>
		Variant(const T& value) requires (can_have<T>() && !is_lvalue_reference_v<T>)
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
				detail::move_assign<Ts...>(m_index, other.m_storage, m_storage);
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
				detail::copy_assign<Ts...>(m_index, other.m_storage, m_storage);
			else
			{
				clear();
				detail::copy_construct<Ts...>(other.m_index, other.m_storage, m_storage);
				m_index = other.m_index;
			}
			return *this;
		}

		template<typename T>
		Variant& operator=(T&& value) requires (can_have<T>() && !is_lvalue_reference_v<T>)
		{
			if (size_t index = detail::index<T, Ts...>(); index == m_index)
				get<T>() = move(value);
			else
			{
				clear();
				new (m_storage) T(move(value));
				m_index = index;
			}
			return *this;
		}

		template<typename T>
		Variant& operator=(const T& value) requires (can_have<T>() && !is_lvalue_reference_v<T>)
		{
			if (size_t index = detail::index<T, Ts...>(); index == m_index)
				get<T>() = value;
			else
			{
				clear();
				new (m_storage) T(value);
				m_index = index;
			}
			return *this;
		}

		template<typename T>
		bool has() const requires (can_have<T>())
		{
			return m_index == detail::index<T, Ts...>();
		}

		template<typename T>
		void set(T&& value) requires (can_have<T>() && !is_lvalue_reference_v<T>)
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
		void set(const T& value) requires (can_have<T>() && !is_lvalue_reference_v<T>)
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
		void set(T value) requires (can_have<T>() && is_lvalue_reference_v<T>)
		{
			clear();
			m_index = detail::index<T, Ts...>();
			*reinterpret_cast<remove_reference_t<T>**>(m_storage) = &value;
		}

		template<typename T>
		T& get() requires (can_have<T>() && !is_lvalue_reference_v<T>)
		{
			ASSERT(has<T>());
			return *reinterpret_cast<T*>(m_storage);
		}

		template<typename T>
		const T& get() const requires (can_have<T>() && !is_lvalue_reference_v<T>)
		{
			ASSERT(has<T>());
			return *reinterpret_cast<const T*>(m_storage);
		}

		template<typename T>
		T get() requires (can_have<T>() && is_lvalue_reference_v<T>)
		{
			ASSERT(has<T>());
			return **reinterpret_cast<remove_reference_t<T>**>(m_storage);
		}

		template<typename T>
		const T get() const requires (can_have<T>() && is_lvalue_reference_v<T>)
		{
			ASSERT(has<T>());
			return **reinterpret_cast<const remove_reference_t<T>**>(m_storage);
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
		alignas(detail::max_align_ref_as_ptr<Ts...>()) uint8_t m_storage[detail::max_size_ref_as_ptr<Ts...>()] {};
		size_t m_index { invalid_index() };
	};

}