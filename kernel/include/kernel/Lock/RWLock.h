#pragma once

#include <kernel/Lock/Mutex.h>
#include <kernel/Lock/LockGuard.h>

namespace Kernel
{

	class RWLock
	{
		BAN_NON_COPYABLE(RWLock);
		BAN_NON_MOVABLE(RWLock);
	public:
		RWLock() = default;

		void rd_lock()
		{
			LockGuard _(m_mutex);
			while (m_writers_waiting > 0 || m_writer != -1)
				m_thread_blocker.block_indefinite(&m_mutex);
			m_readers_active++;
		}

		void rd_unlock()
		{
			LockGuard _(m_mutex);
			if (--m_readers_active == 0)
				m_thread_blocker.unblock();
		}

		void wr_lock()
		{
			if (m_writer == Thread::current_tid())
			{
				m_writer_depth++;
				return;
			}

			LockGuard _(m_mutex);

			m_writers_waiting++;
			while (m_readers_active > 0 || m_writer != -1)
				m_thread_blocker.block_indefinite(&m_mutex);
			m_writers_waiting--;

			m_writer = Thread::current_tid();
			m_writer_depth = 1;
		}

		void wr_unlock()
		{
			if (--m_writer_depth != 0)
				return;
			LockGuard _(m_mutex);
			m_writer = -1;
			m_thread_blocker.unblock();
		}

	private:
		Mutex m_mutex;
		ThreadBlocker m_thread_blocker;
		uint32_t m_readers_active  { 0 };
		uint32_t m_writers_waiting { 0 };
		pid_t m_writer { -1 };
		uint32_t m_writer_depth { 0 };
	};

	class RWLockRDGuard
	{
		BAN_NON_COPYABLE(RWLockRDGuard);
		BAN_NON_MOVABLE(RWLockRDGuard);
	public:
		RWLockRDGuard(RWLock& lock)
			: m_lock(lock)
		{
			m_lock.rd_lock();
		}

		~RWLockRDGuard()
		{
			m_lock.rd_unlock();
		}

	private:
		RWLock& m_lock;
	};

	class RWLockWRGuard
	{
		BAN_NON_COPYABLE(RWLockWRGuard);
		BAN_NON_MOVABLE(RWLockWRGuard);
	public:
		RWLockWRGuard(RWLock& lock)
			: m_lock(lock)
		{
			m_lock.wr_lock();
		}

		~RWLockWRGuard()
		{
			m_lock.wr_unlock();
		}

	private:
		RWLock& m_lock;
	};

}
