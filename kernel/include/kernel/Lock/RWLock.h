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
			while (m_writers_waiting > 0 || m_writer_active)
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
			LockGuard _(m_mutex);
			m_writers_waiting++;
			while (m_readers_active > 0 || m_writer_active)
				m_thread_blocker.block_indefinite(&m_mutex);
			m_writers_waiting--;
			m_writer_active = true;
		}


		void wr_unlock()
		{
			LockGuard _(m_mutex);
			m_writer_active = false;
			m_thread_blocker.unblock();
		}

	private:
		Mutex m_mutex;
		ThreadBlocker m_thread_blocker;
		uint32_t m_readers_active  { 0 };
		uint32_t m_writers_waiting { 0 };
		bool m_writer_active { false };
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
