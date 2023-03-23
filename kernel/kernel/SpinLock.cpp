#include <kernel/SpinLock.h>
#include <kernel/Thread.h>

namespace Kernel
{

	extern "C" void spinlock_lock_asm(int*);
	extern "C" void spinlock_unlock_asm(int*);

	void SpinLock::lock()
	{
		spinlock_lock_asm(&m_lock);
	}

	void SpinLock::unlock()
	{
		spinlock_unlock_asm(&m_lock);
	}

	bool SpinLock::is_locked() const
	{
		return m_lock;
	}

	void RecursiveSpinLock::lock()
	{
		// FIXME: is this thread safe?
		if (m_locker == Thread::current()->tid())
		{
			m_lock_depth++;
		}
		else
		{
			m_lock.lock();
			ASSERT(m_locker == 0);
			m_locker = Thread::current()->tid();
			m_lock_depth = 1;
		}
	}

	void RecursiveSpinLock::unlock()
	{
		m_lock_depth--;
		if (m_lock_depth == 0)
		{
			m_locker = 0;
			m_lock.unlock();
		}
	}

	bool RecursiveSpinLock::is_locked() const
	{
		return m_lock.is_locked();
	}

}