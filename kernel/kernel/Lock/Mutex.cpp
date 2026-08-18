#include <kernel/Lock/Mutex.h>
#include <kernel/Thread.h>

namespace Kernel
{

	bool Mutex::try_lock()
	{
		const auto tid = Thread::current_tid();
		if (tid == m_locker)
			ASSERT(m_lock_depth > 0);
		else
		{
			pid_t expected = -1;
			if (!m_locker.compare_exchange(expected, tid))
				return false;
			ASSERT(m_lock_depth == 0);
			if (tid)
				Thread::current().add_mutex();
		}
		m_lock_depth++;
		return true;
	}

	void Mutex::lock()
	{
		const auto tid = Thread::current_tid();
		if (tid == m_locker)
			ASSERT(m_lock_depth > 0);
		else
		{
			ASSERT(!tid || !Thread::current().has_spinlock());
			pid_t expected = -1;
			while (!m_locker.compare_exchange(expected, tid))
			{
				ASSERT(Processor::get_interrupt_state() == InterruptState::Enabled);
				Processor::yield();
				expected = -1;
			}
			ASSERT(m_lock_depth == 0);
			if (tid)
				Thread::current().add_mutex();
		}
		m_lock_depth++;
	}

	void Mutex::unlock()
	{
		const auto tid = Thread::current_tid();
		ASSERT(m_locker == tid);
		ASSERT(m_lock_depth > 0);
		if (--m_lock_depth == 0)
		{
			m_locker = -1;
			if (tid)
				Thread::current().remove_mutex();
		}
	}

	bool Mutex::is_locked_by_current_thread() const
	{
		return m_locker == Thread::current_tid();
	}

	bool PriorityMutex::try_lock()
	{
		const auto tid = Thread::current_tid();

		if (tid == m_locker)
			ASSERT(m_lock_depth > 0);
		else
		{
			bool has_priority = tid ? !Thread::current().is_userspace() : true;
			pid_t expected = -1;
			if (!(has_priority || m_queue_length == 0) || !m_locker.compare_exchange(expected, tid))
				return false;
			if (has_priority)
				m_queue_length++;
			ASSERT(m_lock_depth == 0);
			if (tid)
				Thread::current().add_mutex();
		}
		m_lock_depth++;
		return true;
	}

	void PriorityMutex::lock()
	{
		const auto tid = Thread::current_tid();

		if (tid == m_locker)
			ASSERT(m_lock_depth > 0);
		else
		{
			ASSERT(!tid || !Thread::current().has_spinlock());
			bool has_priority = tid ? !Thread::current().is_userspace() : true;
			if (has_priority)
				m_queue_length++;
			pid_t expected = -1;
			while (!(has_priority || m_queue_length == 0) || !m_locker.compare_exchange(expected, tid))
			{
				ASSERT(Processor::get_interrupt_state() == InterruptState::Enabled);
				Processor::yield();
				expected = -1;
			}
			ASSERT(m_lock_depth == 0);
			if (tid)
				Thread::current().add_mutex();
		}
		m_lock_depth++;
	}

	void PriorityMutex::unlock()
	{
		const auto tid = Thread::current_tid();
		ASSERT(m_locker == tid);
		ASSERT(m_lock_depth > 0);
		if (--m_lock_depth == 0)
		{
			bool has_priority = tid ? !Thread::current().is_userspace() : true;
			if (has_priority)
				m_queue_length--;
			m_locker = -1;
			if (tid)
				Thread::current().remove_mutex();
		}
	}

	bool PriorityMutex::is_locked_by_current_thread() const
	{
		return m_locker == Thread::current_tid();
	}

}
