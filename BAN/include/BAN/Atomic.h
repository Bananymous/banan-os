#pragma once

namespace BAN
{

	template<typename T, int MEM_ORDER = __ATOMIC_SEQ_CST>
	requires requires { __atomic_always_lock_free(sizeof(T), 0); }
	class Atomic
	{
		Atomic(const Atomic&) = delete;
		Atomic(Atomic&&) = delete;
		Atomic& operator=(const Atomic&) volatile = delete;
		Atomic& operator=(Atomic&&) volatile = delete;

	public:
		constexpr Atomic() : m_value(0) {}
		constexpr Atomic(T val) : m_value(val) {}

		inline T load() const volatile		{ return __atomic_load_n(&m_value, MEM_ORDER); }
		inline void store(T val) volatile	{ __atomic_store_n(&m_value, val, MEM_ORDER); }

		inline T operator=(T val) volatile { store(val); return val; }

		inline operator T() const volatile { return load(); }

		inline T operator+=(T val) volatile { return __atomic_add_fetch(&m_value, val, MEM_ORDER); }
		inline T operator-=(T val) volatile { return __atomic_sub_fetch(&m_value, val, MEM_ORDER); }
		inline T operator&=(T val) volatile { return __atomic_and_fetch(&m_value, val, MEM_ORDER); }
		inline T operator^=(T val) volatile { return __atomic_xor_fetch(&m_value, val, MEM_ORDER); }
		inline T operator|=(T val) volatile { return __atomic_or_fetch(&m_value, val, MEM_ORDER); }

		inline T operator--() volatile { return __atomic_sub_fetch(&m_value, 1, MEM_ORDER); }
		inline T operator++() volatile { return __atomic_add_fetch(&m_value, 1, MEM_ORDER); }

		inline T operator--(int) volatile { return __atomic_fetch_sub(&m_value, 1, MEM_ORDER); }
		inline T operator++(int) volatile { return __atomic_fetch_add(&m_value, 1, MEM_ORDER); }

	private:
		T m_value;
	};

}