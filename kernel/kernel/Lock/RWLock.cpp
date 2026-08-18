
#include <kernel/Lock/BlockableSpinLock.h>
#include <kernel/Lock/RWLock.h>
#include <kernel/Thread.h>

namespace Kernel
{

	void RWLock::rd_lock()
	{
		SpinLockGuard _(m_lock);
		while (m_writers_waiting > 0 || m_writer != -1)
		{
			BlockableSpinLock block(m_lock);
			m_thread_blocker.block_indefinite(&block);
		}
		m_readers_active++;
	}

	void RWLock::rd_unlock()
	{
		SpinLockGuard _(m_lock);
		if (--m_readers_active == 0)
			m_thread_blocker.unblock();
	}

	void RWLock::wr_lock()
	{
		if (m_writer == Thread::current_tid())
		{
			m_writer_depth++;
			return;
		}

		SpinLockGuard _(m_lock);

		m_writers_waiting++;
		while (m_readers_active > 0 || m_writer != -1)
		{
			BlockableSpinLock block(m_lock);
			m_thread_blocker.block_indefinite(&block);
		}
		m_writers_waiting--;

		m_writer = Thread::current_tid();
		m_writer_depth = 1;
	}

	void RWLock::wr_unlock()
	{
		if (--m_writer_depth != 0)
			return;
		SpinLockGuard _(m_lock);
		m_writer = -1;
		m_thread_blocker.unblock();
	}

}
