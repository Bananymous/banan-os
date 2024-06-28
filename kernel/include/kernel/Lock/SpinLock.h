#pragma once

#include <BAN/Assert.h>
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

		InterruptState lock()
		{
			auto state = Processor::get_interrupt_state();
			Processor::set_interrupt_state(InterruptState::Disabled);

			auto id = Processor::current_id();
			ASSERT(m_locker != id);

			ProcessorID expected = PROCESSOR_NONE;
			while (!m_locker.compare_exchange(expected, id, BAN::MemoryOrder::memory_order_acquire))
			{
				__builtin_ia32_pause();
				expected = PROCESSOR_NONE;
			}

			return state;
		}

		void unlock(InterruptState state)
		{
			ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);
			ASSERT(m_locker == Processor::current_id());
			m_locker.store(PROCESSOR_NONE, BAN::MemoryOrder::memory_order_release);
			Processor::set_interrupt_state(state);
		}

		bool current_processor_has_lock() const
		{
			return m_locker == Processor::current_id();
		}

	private:
		BAN::Atomic<ProcessorID> m_locker { PROCESSOR_NONE };
	};

	class RecursiveSpinLock
	{
		BAN_NON_COPYABLE(RecursiveSpinLock);
		BAN_NON_MOVABLE(RecursiveSpinLock);

	public:
		RecursiveSpinLock() = default;

		InterruptState lock()
		{
			auto state = Processor::get_interrupt_state();
			Processor::set_interrupt_state(InterruptState::Disabled);

			auto id = Processor::current_id();
			if (m_locker == id)
				ASSERT(m_lock_depth > 0);
			else
			{
				ProcessorID expected = PROCESSOR_NONE;
				while (!m_locker.compare_exchange(expected, id, BAN::MemoryOrder::memory_order_acquire))
				{
					__builtin_ia32_pause();
					expected = PROCESSOR_NONE;
				}
				ASSERT(m_lock_depth == 0);
			}

			m_lock_depth++;

			return state;
		}

		void unlock(InterruptState state)
		{
			ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);
			ASSERT(m_locker == Processor::current_id());
			ASSERT(m_lock_depth > 0);
			if (--m_lock_depth == 0)
				m_locker.store(PROCESSOR_NONE, BAN::MemoryOrder::memory_order_release);
			Processor::set_interrupt_state(state);
		}

		bool current_processor_has_lock() const
		{
			return m_locker == Processor::current_id();
		}

	private:
		BAN::Atomic<ProcessorID>	m_locker { PROCESSOR_NONE };
		uint32_t					m_lock_depth { 0 };
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
