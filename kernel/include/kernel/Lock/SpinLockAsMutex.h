#pragma once

#include <kernel/Lock/SpinLock.h>
#include <kernel/Lock/Mutex.h>

namespace Kernel
{

	// FIXME: These classes are HACKS to allow passing spinlock
	//        to unblock functions. Write a better API that either
	//        allows passing spinlocks or do something cleaner that
	//        whatever shit this is

	template<typename Lock>
	class SpinLockAsMutex : public BaseMutex
	{
	public:
		SpinLockAsMutex(Lock& lock, InterruptState state)
			: m_lock(lock)
			, m_lock_depth(lock.lock_depth())
			, m_state(state)
			, m_locker(Thread::current_tid())
		{
			ASSERT(m_lock.current_processor_has_lock());
		}

		void lock() override
		{
			m_lock.lock();
			m_lock_depth++;
		}

		bool try_lock() override
		{
			lock();
			return true;
		}

		void unlock() override
		{
			m_lock.unlock(--m_lock_depth ? InterruptState::Disabled : m_state);
		}

		pid_t locker() const override { return is_locked() ? m_locker : -1; }
		bool is_locked() const override { return m_lock_depth; }
		uint32_t lock_depth() const override { return m_lock_depth; }

	private:
		SpinLock& m_lock;
		uint32_t m_lock_depth { 0 };
		InterruptState m_state;
		const pid_t m_locker;
	};

	template<typename Lock>
	class SpinLockGuardAsMutex : public SpinLockAsMutex<Lock>
	{
	public:
		SpinLockGuardAsMutex(SpinLockGuard<Lock>& guard)
			: SpinLockAsMutex<Lock>(guard.m_lock, guard.m_state)
		{}
	};

}
