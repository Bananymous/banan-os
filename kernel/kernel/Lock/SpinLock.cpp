#include <kernel/InterruptController.h>
#include <kernel/Lock/SpinLock.h>
#include <kernel/Scheduler.h>

// FIXME: try to move these to header

namespace Kernel
{

	InterruptState SpinLock::lock()
	{
		auto tid = Scheduler::current_tid();
		ASSERT_NEQ(m_locker.load(), tid);

		auto state = get_interrupt_state();
		set_interrupt_state(InterruptState::Disabled);

		if (!m_locker.compare_exchange(-1, tid))
			ASSERT_NOT_REACHED();

		return state;
	}

	void SpinLock::unlock(InterruptState state)
	{
		ASSERT_EQ(m_locker.load(), Scheduler::current_tid());
		m_locker.store(-1);
		set_interrupt_state(state);
	}

	InterruptState RecursiveSpinLock::lock()
	{
		auto tid = Scheduler::current_tid();

		auto state = get_interrupt_state();
		set_interrupt_state(InterruptState::Disabled);

		if (tid == m_locker)
			ASSERT_GT(m_lock_depth, 0);
		else
		{
			if (!m_locker.compare_exchange(-1, tid))
				ASSERT_NOT_REACHED();
			ASSERT_EQ(m_lock_depth, 0);
		}

		m_lock_depth++;

		return state;
	}

	void RecursiveSpinLock::unlock(InterruptState state)
	{
		auto tid = Scheduler::current_tid();
		ASSERT_EQ(m_locker.load(), tid);
		ASSERT_GT(m_lock_depth, 0);
		if (--m_lock_depth == 0)
			m_locker = -1;
		set_interrupt_state(state);
	}

}
