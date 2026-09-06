#pragma once

#include <BAN/Assert.h>
#include <BAN/Atomic.h>
#include <BAN/NoCopyMove.h>
#include <kernel/InterruptState.h>
#include <kernel/ProcessorID.h>

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

		bool try_lock_interrupts_disabled();

		void unlock(InterruptState state);

		uint32_t lock_depth() const;

		bool current_processor_has_lock() const;

	private:
		BAN::Atomic<ProcessorID::value_type> m_locker { PROCESSOR_NONE.as_u32() };
	};

	class RecursiveSpinLock
	{
		BAN_NON_COPYABLE(RecursiveSpinLock);
		BAN_NON_MOVABLE(RecursiveSpinLock);

	public:
		RecursiveSpinLock() = default;

		InterruptState lock();

		bool try_lock_interrupts_disabled();

		void unlock(InterruptState state);

		uint32_t lock_depth() const;

		bool current_processor_has_lock() const;

	private:
		BAN::Atomic<ProcessorID::value_type> m_locker { PROCESSOR_NONE.as_u32() };
		uint32_t                             m_lock_depth { 0 };
	};

	template<typename Lock>
	class SpinLockGuard
	{
		BAN_NON_COPYABLE(SpinLockGuard);
		BAN_NON_MOVABLE(SpinLockGuard);

	public:
		SpinLockGuard(Lock& lock)
			: m_lock(lock)
			, m_state(lock.lock())
		{ }

		~SpinLockGuard()
		{
			m_lock.unlock(m_state);
		}

	private:
		Lock& m_lock;
		const InterruptState m_state;
	};

}
