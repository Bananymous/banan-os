#pragma once

#include <BAN/Atomic.h>
#include <BAN/NoCopyMove.h>
#include <kernel/Interrupts.h>

#include <sys/types.h>

namespace Kernel
{

	class SpinLock
	{
		BAN_NON_COPYABLE(SpinLock);
		BAN_NON_MOVABLE(SpinLock);

	public:
		SpinLock() = default;

		InterruptState lock();
		void unlock(InterruptState state);

	private:
		BAN::Atomic<pid_t> m_locker { -1 };
	};

	class RecursiveSpinLock
	{
		BAN_NON_COPYABLE(RecursiveSpinLock);
		BAN_NON_MOVABLE(RecursiveSpinLock);

	public:
		RecursiveSpinLock() = default;

		InterruptState lock();
		void unlock(InterruptState state);

	private:
		BAN::Atomic<pid_t> m_locker { -1 };
		uint32_t m_lock_depth { 0 };
	};

	class SpinLockUnsafe
	{
		BAN_NON_COPYABLE(SpinLockUnsafe);
		BAN_NON_MOVABLE(SpinLockUnsafe);

	public:
		SpinLockUnsafe() = default;

		InterruptState lock()
		{
			auto state = get_interrupt_state();
			set_interrupt_state(InterruptState::Disabled);

			while (!m_locked.compare_exchange(false, true))
				__builtin_ia32_pause();

			return state;
		}

		void unlock(InterruptState state)
		{
			m_locked.store(false);
			set_interrupt_state(state);
		}

		bool is_locked() const { return m_locked; }

	private:
		BAN::Atomic<bool> m_locked;
	};

	template<typename Lock>
	class SpinLockGuard
	{
		BAN_NON_COPYABLE(SpinLockGuard);
		BAN_NON_MOVABLE(SpinLockGuard);

	public:
		SpinLockGuard(Lock& lock)
			: m_lock(lock)
		{
			m_state = m_lock.lock();
		}

		~SpinLockGuard()
		{
			m_lock.unlock(m_state);
		}

	private:
		Lock& m_lock;
		InterruptState m_state;
	};

}
