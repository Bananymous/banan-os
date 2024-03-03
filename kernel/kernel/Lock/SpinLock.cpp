#include <kernel/InterruptController.h>
#include <kernel/Lock/SpinLock.h>
#include <kernel/Scheduler.h>

// FIXME: try to move these to header

namespace Kernel
{

	InterruptState SpinLock::lock()
	{
		auto id = Processor::current_id();
		ASSERT_NEQ(m_locker.load(), id);

		auto state = Processor::get_interrupt_state();
		Processor::set_interrupt_state(InterruptState::Disabled);

		while (!m_locker.compare_exchange(PROCESSOR_NONE, id, BAN::MemoryOrder::memory_order_acquire))
			__builtin_ia32_pause();

		return state;
	}

	void SpinLock::unlock(InterruptState state)
	{
		ASSERT_EQ(m_locker.load(), Processor::current_id());
		m_locker.store(PROCESSOR_NONE, BAN::MemoryOrder::memory_order_release);
		Processor::set_interrupt_state(state);
	}

	InterruptState RecursiveSpinLock::lock()
	{
		auto id = Processor::current_id();

		auto state = Processor::get_interrupt_state();
		Processor::set_interrupt_state(InterruptState::Disabled);

		if (id == m_locker)
			ASSERT_GT(m_lock_depth, 0);
		else
		{
			while (!m_locker.compare_exchange(PROCESSOR_NONE, id, BAN::MemoryOrder::memory_order_acquire))
				__builtin_ia32_pause();
			ASSERT_EQ(m_lock_depth, 0);
		}

		m_lock_depth++;

		return state;
	}

	void RecursiveSpinLock::unlock(InterruptState state)
	{
		ASSERT_EQ(m_locker.load(), Processor::current_id());
		ASSERT_GT(m_lock_depth, 0);
		if (--m_lock_depth == 0)
			m_locker.store(PROCESSOR_NONE, BAN::MemoryOrder::memory_order_release);
		Processor::set_interrupt_state(state);
	}

}
