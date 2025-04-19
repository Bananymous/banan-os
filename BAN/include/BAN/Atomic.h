#pragma once

#include <BAN/Traits.h>

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

	template<typename T> concept atomic_c = is_integral_v<T> || is_pointer_v<T>;
	template<typename T> concept atomic_lockfree_c = (is_integral_v<T> || is_pointer_v<T>) && __atomic_always_lock_free(sizeof(T), 0);

	template<atomic_lockfree_c T, atomic_c U>
	inline void atomic_store(T& obj, U value, MemoryOrder mem_order = MemoryOrder::memory_order_seq_cst) { __atomic_store_n(&obj, value, mem_order); }
	template<atomic_lockfree_c T>
	inline T atomic_load(T& obj, MemoryOrder mem_order = MemoryOrder::memory_order_seq_cst) { return __atomic_load_n(&obj, mem_order); }

	template<atomic_lockfree_c T, atomic_c U>
	inline T atomic_exchange(T& obj, U value, MemoryOrder mem_order = MemoryOrder::memory_order_seq_cst) { return __atomic_exchange_n(&obj, value, mem_order); }
	template<atomic_lockfree_c T, atomic_lockfree_c U, atomic_c V>
	inline bool atomic_compare_exchange(T& obj, U& expected, V value, MemoryOrder mem_order = MemoryOrder::memory_order_seq_cst) { return __atomic_compare_exchange_n(&obj, &expected, value, false, mem_order, mem_order); }

#define DECL_ATOMIC_INLINE template<atomic_lockfree_c T, atomic_c U> inline
	DECL_ATOMIC_INLINE T atomic_add_fetch (T& obj, U value, MemoryOrder mem_order = MemoryOrder::memory_order_seq_cst) { return __atomic_add_fetch (&obj, value, mem_order); }
	DECL_ATOMIC_INLINE T atomic_sub_fetch (T& obj, U value, MemoryOrder mem_order = MemoryOrder::memory_order_seq_cst) { return __atomic_sub_fetch (&obj, value, mem_order); }
	DECL_ATOMIC_INLINE T atomic_and_fetch (T& obj, U value, MemoryOrder mem_order = MemoryOrder::memory_order_seq_cst) { return __atomic_and_fetch (&obj, value, mem_order); }
	DECL_ATOMIC_INLINE T atomic_xor_fetch (T& obj, U value, MemoryOrder mem_order = MemoryOrder::memory_order_seq_cst) { return __atomic_xor_fetch (&obj, value, mem_order); }
	DECL_ATOMIC_INLINE T atomic_or_fetch  (T& obj, U value, MemoryOrder mem_order = MemoryOrder::memory_order_seq_cst) { return __atomic_or_fetch  (&obj, value, mem_order); }
	DECL_ATOMIC_INLINE T atomic_nand_fetch(T& obj, U value, MemoryOrder mem_order = MemoryOrder::memory_order_seq_cst) { return __atomic_nand_fetch(&obj, value, mem_order); }

	DECL_ATOMIC_INLINE T atomic_fetch_add (T& obj, U value, MemoryOrder mem_order = MemoryOrder::memory_order_seq_cst) { return __atomic_fetch_add (&obj, value, mem_order); }
	DECL_ATOMIC_INLINE T atomic_fetch_sub (T& obj, U value, MemoryOrder mem_order = MemoryOrder::memory_order_seq_cst) { return __atomic_fetch_sub (&obj, value, mem_order); }
	DECL_ATOMIC_INLINE T atomic_fetch_and (T& obj, U value, MemoryOrder mem_order = MemoryOrder::memory_order_seq_cst) { return __atomic_fetch_and (&obj, value, mem_order); }
	DECL_ATOMIC_INLINE T atomic_fetch_xor (T& obj, U value, MemoryOrder mem_order = MemoryOrder::memory_order_seq_cst) { return __atomic_fetch_xor (&obj, value, mem_order); }
	DECL_ATOMIC_INLINE T atomic_fetch_or  (T& obj, U value, MemoryOrder mem_order = MemoryOrder::memory_order_seq_cst) { return __atomic_fetch_or  (&obj, value, mem_order); }
	DECL_ATOMIC_INLINE T atomic_fetch_nand(T& obj, U value, MemoryOrder mem_order = MemoryOrder::memory_order_seq_cst) { return __atomic_fetch_nand(&obj, value, mem_order); }
#undef DECL_ATOMIC_INLINE

	template<atomic_lockfree_c T, MemoryOrder MEM_ORDER = MemoryOrder::memory_order_seq_cst>
	class Atomic
	{
		Atomic(const Atomic&) = delete;
		Atomic(Atomic&&) = delete;
		Atomic& operator=(const Atomic&) volatile = delete;
		Atomic& operator=(Atomic&&) volatile = delete;

	public:
		constexpr Atomic() : m_value(0) {}
		constexpr Atomic(T val) : m_value(val) {}

		inline T load(MemoryOrder mem_order = MEM_ORDER) const volatile			{ return atomic_load(m_value, mem_order); }
		inline void store(T val, MemoryOrder mem_order = MEM_ORDER) volatile	{ atomic_store(m_value, val, mem_order); }

		inline T operator=(T val) volatile { store(val); return val; }

		inline operator T() const volatile { return load(); }

		inline T operator+=(T val) volatile { return atomic_add_fetch(m_value, val, MEM_ORDER); }
		inline T operator-=(T val) volatile { return atomic_sub_fetch(m_value, val, MEM_ORDER); }
		inline T operator&=(T val) volatile { return atomic_and_fetch(m_value, val, MEM_ORDER); }
		inline T operator^=(T val) volatile { return atomic_xor_fetch(m_value, val, MEM_ORDER); }
		inline T operator|=(T val) volatile { return atomic_or_fetch(m_value, val, MEM_ORDER); }

		inline T operator--() volatile { return atomic_sub_fetch(m_value, 1, MEM_ORDER); }
		inline T operator++() volatile { return atomic_add_fetch(m_value, 1, MEM_ORDER); }

		inline T operator--(int) volatile { return atomic_fetch_sub(m_value, 1, MEM_ORDER); }
		inline T operator++(int) volatile { return atomic_fetch_add(m_value, 1, MEM_ORDER); }

		inline bool compare_exchange(T& expected, T desired, MemoryOrder mem_order = MEM_ORDER) volatile { return atomic_compare_exchange(m_value, expected, desired, mem_order); }
		inline T exchange(T desired, MemoryOrder mem_order = MEM_ORDER) volatile { return atomic_exchange(m_value, desired, mem_order); };

		inline T add_fetch (T val, MemoryOrder mem_order = MEM_ORDER) volatile { return atomic_add_fetch (m_value, val, mem_order); }
		inline T sub_fetch (T val, MemoryOrder mem_order = MEM_ORDER) volatile { return atomic_sub_fetch (m_value, val, mem_order); }
		inline T and_fetch (T val, MemoryOrder mem_order = MEM_ORDER) volatile { return atomic_and_fetch (m_value, val, mem_order); }
		inline T xor_fetch (T val, MemoryOrder mem_order = MEM_ORDER) volatile { return atomic_xor_fetch (m_value, val, mem_order); }
		inline T or_fetch  (T val, MemoryOrder mem_order = MEM_ORDER) volatile { return atomic_or_fetch  (m_value, val, mem_order); }
		inline T nand_fetch(T val, MemoryOrder mem_order = MEM_ORDER) volatile { return atomic_nand_fetch(m_value, val, mem_order); }

		inline T fetch_add (T val, MemoryOrder mem_order = MEM_ORDER) volatile { return atomic_fetch_add (m_value, val, mem_order); }
		inline T fetch_sub (T val, MemoryOrder mem_order = MEM_ORDER) volatile { return atomic_fetch_sub (m_value, val, mem_order); }
		inline T fetch_and (T val, MemoryOrder mem_order = MEM_ORDER) volatile { return atomic_fetch_and (m_value, val, mem_order); }
		inline T fetch_xor (T val, MemoryOrder mem_order = MEM_ORDER) volatile { return atomic_fetch_xor (m_value, val, mem_order); }
		inline T fetch_or  (T val, MemoryOrder mem_order = MEM_ORDER) volatile { return atomic_fetch_or  (m_value, val, mem_order); }
		inline T fetch_nand(T val, MemoryOrder mem_order = MEM_ORDER) volatile { return atomic_fetch_nand(m_value, val, mem_order); }

	private:
		T m_value;
	};

}
