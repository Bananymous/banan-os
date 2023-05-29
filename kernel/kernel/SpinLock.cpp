#include <kernel/Scheduler.h>
#include <kernel/SpinLock.h>

namespace Kernel
{

	void SpinLock::lock()
	{
		while (__sync_lock_test_and_set(&m_lock, 1))
			while (m_lock)
				__builtin_ia32_pause();
	}

	void SpinLock::unlock()
	{
		__sync_lock_release(&m_lock);
	}

	bool SpinLock::is_locked() const
	{
		return m_lock;
	}

	void RecursiveSpinLock::lock()
	{
		pid_t tid = Scheduler::current_tid();

		while (true)
		{
			// Wait for us to be the locker or the lock being free
			while (m_locker != -1 && m_locker != tid)
				__builtin_ia32_pause();

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

}