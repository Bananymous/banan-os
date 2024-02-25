#include <kernel/Lock/Mutex.h>
#include <kernel/Scheduler.h>

namespace Kernel
{

	void Mutex::lock()
	{
		auto tid = Scheduler::current_tid();
		if (tid != m_locker)
			while (!m_locker.compare_exchange(-1, tid))
				Scheduler::get().reschedule();
		m_lock_depth++;
	}

	bool Mutex::try_lock()
	{
		auto tid = Scheduler::current_tid();
		if (tid != m_locker)
			if (!m_locker.compare_exchange(-1, tid))
				return false;
		m_lock_depth++;
		return true;
	}

	void Mutex::unlock()
	{
		ASSERT_EQ(m_locker.load(), Scheduler::current_tid());
		if (--m_lock_depth == 0)
			m_locker = -1;
	}

	void PriorityMutex::lock()
	{
		const auto tid = Scheduler::current_tid();
		const bool has_priority = tid ? !Thread::current().is_userspace() : true;
		if (has_priority)
			m_queue_depth++;
		if (tid != m_locker)
			while ((!has_priority && m_queue_depth > 0) || !m_locker.compare_exchange(-1, tid))
				asm volatile("pause");
		m_lock_depth++;
	}

	bool PriorityMutex::try_lock()
	{
		const auto tid = Scheduler::current_tid();
		const bool has_priority = tid ? !Thread::current().is_userspace() : true;
		if (tid != m_locker)
			while ((!has_priority && m_queue_depth > 0) || !m_locker.compare_exchange(-1, tid))
				return false;
		if (has_priority)
			m_queue_depth++;
		m_lock_depth++;
		return true;
	}

	void PriorityMutex::unlock()
	{
		const auto tid = Scheduler::current_tid();
		const bool has_priority = tid ? !Thread::current().is_userspace() : true;
		if (has_priority)
			m_queue_depth--;
		if (--m_lock_depth)
			m_locker = -1;
	}

}
