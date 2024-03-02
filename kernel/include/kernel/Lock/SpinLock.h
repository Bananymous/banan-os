#pragma once

#include <BAN/Atomic.h>
#include <BAN/NoCopyMove.h>
#include <kernel/Processor.h>

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
		BAN::Atomic<ProcessorID>	m_locker { PROCESSOR_NONE };
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
		BAN::Atomic<ProcessorID>	m_locker { PROCESSOR_NONE };
		uint32_t					m_lock_depth { 0 };
	};

	class SpinLockUnsafe
	{
		BAN_NON_COPYABLE(SpinLockUnsafe);
		BAN_NON_MOVABLE(SpinLockUnsafe);

	public:
		SpinLockUnsafe() = default;

		InterruptState lock()
		{
			auto id = get_processor_id();

			auto state = get_interrupt_state();
			set_interrupt_state(InterruptState::Disabled);

			while (!m_locker.compare_exchange(PROCESSOR_NONE, id, BAN::MemoryOrder::memory_order_acquire))
				__builtin_ia32_pause();

			return state;
		}

		void unlock(InterruptState state)
		{
			m_locker.store(PROCESSOR_NONE, BAN::MemoryOrder::memory_order_release);
			set_interrupt_state(state);
		}

		bool is_locked() const { return m_locker != PROCESSOR_NONE; }

	private:
		BAN::Atomic<ProcessorID> m_locker { PROCESSOR_NONE };
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
