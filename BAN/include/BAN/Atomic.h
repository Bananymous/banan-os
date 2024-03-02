#pragma once

namespace BAN
{

	enum MemoryOrder
	{
		memory_order_relaxed = __ATOMIC_RELAXED,
		memory_order_consume = __ATOMIC_CONSUME,
		memory_order_acquire = __ATOMIC_ACQUIRE,
		memory_order_release = __ATOMIC_RELEASE,
		memory_order_acq_rel = __ATOMIC_ACQ_REL,
		memory_order_seq_cst = __ATOMIC_SEQ_CST,
	};

	template<typename T, MemoryOrder MEM_ORDER = MemoryOrder::memory_order_seq_cst>
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

		inline T load(MemoryOrder mem_order = MEM_ORDER) const volatile			{ return __atomic_load_n(&m_value, mem_order); }
		inline void store(T val, MemoryOrder mem_order = MEM_ORDER) volatile	{ __atomic_store_n(&m_value, val, mem_order); }

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

		inline bool compare_exchange(T expected, T desired, MemoryOrder mem_order = MEM_ORDER) volatile { return __atomic_compare_exchange_n(&m_value, &expected, desired, false, mem_order, mem_order); }

	private:
		T m_value;
	};

}
