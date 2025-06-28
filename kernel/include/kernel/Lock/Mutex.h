#pragma once

#include <BAN/Atomic.h>
#include <BAN/NoCopyMove.h>
#include <kernel/Thread.h>

#include <sys/types.h>

namespace Kernel
{

	class BaseMutex
	{
	public:
		virtual void lock() = 0;
		virtual bool try_lock() = 0;
		virtual void unlock() = 0;

		virtual pid_t locker() const = 0;
		virtual bool is_locked() const = 0;
		virtual uint32_t lock_depth() const = 0;
	};

	class Mutex : public BaseMutex
	{
		BAN_NON_COPYABLE(Mutex);
		BAN_NON_MOVABLE(Mutex);

	public:
		Mutex() = default;

		void lock() override
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
					Processor::yield();
					expected = -1;
				}
				ASSERT(m_lock_depth == 0);
				if (tid)
					Thread::current().add_mutex();
			}
			m_lock_depth++;
		}

		bool try_lock() override
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

		void unlock() override
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

		pid_t locker() const override { return m_locker; }
		bool is_locked() const override { return m_locker != -1; }
		uint32_t lock_depth() const override { return m_lock_depth; }

	private:
		BAN::Atomic<pid_t>	m_locker		{ -1 };
		uint32_t			m_lock_depth	{  0 };
	};

	class PriorityMutex : public BaseMutex
	{
		BAN_NON_COPYABLE(PriorityMutex);
		BAN_NON_MOVABLE(PriorityMutex);

	public:
		PriorityMutex() = default;

		void lock() override
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
					Processor::yield();
					expected = -1;
				}
				ASSERT(m_lock_depth == 0);
				if (tid)
					Thread::current().add_mutex();
			}
			m_lock_depth++;
		}

		bool try_lock() override
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

		void unlock() override
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

		pid_t locker() const override { return m_locker; }
		bool is_locked() const override { return m_locker != -1; }
		uint32_t lock_depth() const override { return m_lock_depth; }

	private:
		BAN::Atomic<pid_t>		m_locker		{ -1 };
		uint32_t				m_lock_depth	{  0 };
		BAN::Atomic<uint32_t>	m_queue_length	{  0 };
	};

}
