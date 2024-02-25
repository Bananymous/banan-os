#pragma once

#include <BAN/Atomic.h>
#include <BAN/NoCopyMove.h>

#include <sys/types.h>

namespace Kernel
{

	class Mutex
	{
		BAN_NON_COPYABLE(Mutex);
		BAN_NON_MOVABLE(Mutex);

	public:
		Mutex() = default;

		void lock();
		bool try_lock();
		void unlock();

		pid_t locker() const { return m_locker; }
		bool is_locked() const { return m_locker != -1; }
		uint32_t lock_depth() const { return m_lock_depth; }

	private:
		BAN::Atomic<pid_t> m_locker { -1 };
		uint32_t m_lock_depth { 0 };
	};

	class PriorityMutex
	{
		BAN_NON_COPYABLE(PriorityMutex);
		BAN_NON_MOVABLE(PriorityMutex);

	public:
		PriorityMutex() = default;

		void lock();
		bool try_lock();
		void unlock();

		pid_t locker() const { return m_locker; }
		bool is_locked() const { return m_locker != -1; }
		uint32_t lock_depth() const { return m_lock_depth; }

	private:
		BAN::Atomic<pid_t> m_locker { -1 };
		uint32_t m_lock_depth { 0 };
		BAN::Atomic<uint32_t> m_queue_depth { 0 };
	};

}
