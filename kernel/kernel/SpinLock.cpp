#include <kernel/Scheduler.h>
#include <kernel/SpinLock.h>
#include <kernel/CriticalScope.h>

namespace Kernel
{

	void SpinLock::lock()
	{
		pid_t tid = Scheduler::current_tid();
		while (true)
		{
			{
				CriticalScope _;
				ASSERT(m_locker != tid);
				if (m_locker == -1)
				{
					m_locker = tid;
					break;
				}
			}
			Scheduler::get().reschedule();
		}
	}

	void SpinLock::unlock()
	{
		CriticalScope _;
		ASSERT(m_locker == Scheduler::current_tid());
		m_locker = -1;
	}

	bool SpinLock::is_locked() const
	{
		CriticalScope _;
		return m_locker != -1;
	}

	void RecursiveSpinLock::lock()
	{
		pid_t tid = Scheduler::current_tid();

		while (true)
		{
			m_lock.lock();
			if (m_locker == tid)
			{
				m_lock_depth++;
				break;
			}
			if (m_locker == -1)
			{
				m_locker = tid;
				m_lock_depth = 1;
				break;
			}
			m_lock.unlock();
		}

		m_lock.unlock();
	}

	void RecursiveSpinLock::unlock()
	{
		m_lock.lock();
		
		ASSERT(m_lock_depth > 0);
		ASSERT(m_locker == Scheduler::current_tid());

		m_lock_depth--;

		if (m_lock_depth == 0)
			m_locker = -1;
		
		m_lock.unlock();
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
		{
			m_lock.lock();
			m_queue_length++;
			m_lock.unlock();
		}

		while (true)
		{
			m_lock.lock();
			if (m_locker == tid)
			{
				m_lock_depth++;
				break;
			}
			if (m_locker == -1 && (has_priority || m_queue_length == 0))
			{
				m_locker = tid;
				m_lock_depth = 1;
				break;
			}
			m_lock.unlock();
		}

		m_lock.unlock();
	}

	void RecursivePrioritySpinLock::unlock()
	{
		m_lock.lock();
		
		ASSERT(m_lock_depth > 0);
		ASSERT(m_locker == Scheduler::current_tid());

		bool has_priority = !Thread::current().is_userspace();
		if (has_priority)
			m_queue_length--;

		m_lock_depth--;

		if (m_lock_depth == 0)
			m_locker = -1;
		
		m_lock.unlock();
	}

	bool RecursivePrioritySpinLock::is_locked() const
	{
		return m_locker != -1;
	}

}