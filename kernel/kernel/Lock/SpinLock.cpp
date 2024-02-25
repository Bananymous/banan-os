#include <kernel/Lock/SpinLock.h>
#include <kernel/Scheduler.h>

namespace Kernel
{

	static inline uintptr_t get_flags_and_disable_interrupts()
	{
		uintptr_t flags;
		asm volatile("pushf; cli; pop %0" : "=r"(flags) :: "memory");
		return flags;
	}

	static inline void restore_flags(uintptr_t flags)
	{
		asm volatile("push %0; popf" :: "rm"(flags) : "memory", "cc");
	}

	void SpinLock::lock()
	{
		const auto tid = Scheduler::current_tid();
		ASSERT_NEQ(m_locker.load(), tid);
		while (!m_locker.compare_exchange(-1, tid))
			__builtin_ia32_pause();
		m_flags = get_flags_and_disable_interrupts();
	}

	bool SpinLock::try_lock()
	{
		const auto tid = Scheduler::current_tid();
		ASSERT_NEQ(m_locker.load(), tid);
		if (!m_locker.compare_exchange(-1, tid))
			return false;
		m_flags = get_flags_and_disable_interrupts();
		return true;
	}

	void SpinLock::unlock()
	{
		ASSERT_EQ(m_locker.load(), Scheduler::current_tid());
		restore_flags(m_flags);
		m_locker = -1;
	}

	void RecursiveSpinLock::lock()
	{
		auto tid = Scheduler::current_tid();
		if (m_locker != tid)
		{
			while (!m_locker.compare_exchange(-1, tid))
				__builtin_ia32_pause();
			m_flags = get_flags_and_disable_interrupts();
		}
		m_lock_depth++;
	}

	bool RecursiveSpinLock::try_lock()
	{
		auto tid = Scheduler::current_tid();
		if (m_locker != tid)
		{
			if (!m_locker.compare_exchange(-1, tid))
				return false;
			m_flags = get_flags_and_disable_interrupts();
		}
		m_lock_depth++;
		return true;
	}

	void RecursiveSpinLock::unlock()
	{
		ASSERT_EQ(m_locker.load(), Scheduler::current_tid());
		if (--m_lock_depth == 0)
		{
			restore_flags(m_flags);
			m_locker = -1;
		}
	}

}
