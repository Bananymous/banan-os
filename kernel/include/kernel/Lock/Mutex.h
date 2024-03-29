#pragma once

#include <BAN/Atomic.h>
#include <BAN/NoCopyMove.h>
#include <kernel/Scheduler.h>

#include <sys/types.h>

namespace Kernel
{

	class Mutex
	{
		BAN_NON_COPYABLE(Mutex);
		BAN_NON_MOVABLE(Mutex);

	public:
		Mutex() = default;

		void lock()
		{
			auto tid = Scheduler::current_tid();
			if (tid == m_locker)
				ASSERT(m_lock_depth > 0);
			else
			{
				while (!m_locker.compare_exchange(-1, tid))
					Scheduler::get().yield();
				ASSERT(m_lock_depth == 0);
			}
			m_lock_depth++;
		}

		bool try_lock()
		{
			auto tid = Scheduler::current_tid();
			if (tid == m_locker)
				ASSERT(m_lock_depth > 0);
			else
			{
				if (!m_locker.compare_exchange(-1, tid))
					return false;
				ASSERT(m_lock_depth == 0);
			}
			m_lock_depth++;
		}

		void unlock()
		{
			ASSERT(m_locker == Scheduler::current_tid());
			ASSERT(m_lock_depth > 0);
			if (--m_lock_depth == 0)
				m_locker = -1;
		}

		pid_t locker() const { return m_locker; }
		bool is_locked() const { return m_locker != -1; }
		uint32_t lock_depth() const { return m_lock_depth; }

	private:
		BAN::Atomic<pid_t>	m_locker		{ -1 };
		uint32_t			m_lock_depth	{  0 };
	};

	class PriorityMutex
	{
		BAN_NON_COPYABLE(PriorityMutex);
		BAN_NON_MOVABLE(PriorityMutex);

	public:
		PriorityMutex() = default;

		void lock()
		{
			auto tid = Scheduler::current_tid();
			if (tid == m_locker)
				ASSERT(m_lock_depth > 0);
			else
			{
				bool has_priority = tid ? !Thread::current().is_userspace() : true;
				if (has_priority)
					m_queue_length++;
				while (!(has_priority || m_queue_length == 0) || !m_locker.compare_exchange(-1, tid))
					Scheduler::get().yield();
				ASSERT(m_lock_depth == 0);
			}
			m_lock_depth++;
		}

		bool try_lock()
		{
			auto tid = Scheduler::current_tid();
			if (tid == m_locker)
				ASSERT(m_lock_depth > 0);
			else
			{
				bool has_priority = tid ? !Thread::current().is_userspace() : true;
				if (!(has_priority || m_queue_length == 0) || !m_locker.compare_exchange(-1, tid))
					return false;
				if (has_priority)
					m_queue_length++;
				ASSERT(m_lock_depth == 0);
			}
			m_lock_depth++;
		}

		void unlock()
		{
			auto tid = Scheduler::current_tid();
			ASSERT(m_locker == tid);
			ASSERT(m_lock_depth > 0);
			if (--m_lock_depth == 0)
			{
				bool has_priority = tid ? !Thread::current().is_userspace() : true;
				if (has_priority)
					m_queue_length--;
				m_locker = -1;
			}
		}

		pid_t locker() const { return m_locker; }
		bool is_locked() const { return m_locker != -1; }
		uint32_t lock_depth() const { return m_lock_depth; }

	private:
		BAN::Atomic<pid_t>		m_locker		{ -1 };
		uint32_t				m_lock_depth	{  0 };
		BAN::Atomic<uint32_t>	m_queue_length	{  0 };
	};

}
