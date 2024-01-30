#include <kernel/Scheduler.h>
#include <kernel/SpinLock.h>

namespace Kernel
{

	void SpinLock::lock()
	{
		pid_t tid = Scheduler::current_tid();
		while (!m_locker.compare_exchange(-1, tid))
			Scheduler::get().reschedule();
	}

	void SpinLock::unlock()
	{
		ASSERT(m_locker == Scheduler::current_tid());
		m_locker = -1;
	}

	bool SpinLock::is_locked() const
	{
		return m_locker != -1;
	}

	void RecursiveSpinLock::lock()
	{
		pid_t tid = Scheduler::current_tid();
		if (m_locker != tid)
		{
			while (!m_locker.compare_exchange(-1, tid))
				Scheduler::get().reschedule();
			ASSERT(m_lock_depth == 0);
		}
		m_lock_depth++;
	}

	void RecursiveSpinLock::unlock()
	{
		ASSERT(m_lock_depth > 0);
		ASSERT(m_locker == Scheduler::current_tid());
		if (--m_lock_depth == 0)
			m_locker = -1;
	}

	bool RecursiveSpinLock::is_locked() const
	{
		return m_locker != -1;
	}

	void RecursivePrioritySpinLock::lock()
	{
		pid_t tid = Scheduler::current_tid();

		bool has_priority = !Thread::current().is_userspace();
		if (has_priority)
			m_queue_length++;

		if (m_locker != tid)
		{
			while (!((has_priority || m_queue_length == 0) && m_locker.compare_exchange(-1, tid)))
				Scheduler::get().reschedule();
			ASSERT(m_lock_depth == 0);
		}
		m_lock_depth++;
	}

	void RecursivePrioritySpinLock::unlock()
	{
		ASSERT(m_lock_depth > 0);
		ASSERT(m_locker == Scheduler::current_tid());

		bool has_priority = !Thread::current().is_userspace();
		if (has_priority)
			m_queue_length--;

		if (--m_lock_depth == 0)
			m_locker = -1;
	}

	bool RecursivePrioritySpinLock::is_locked() const
	{
		return m_locker != -1;
	}

}
