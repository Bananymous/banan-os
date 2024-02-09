#pragma once

#include <BAN/Atomic.h>
#include <BAN/NoCopyMove.h>

#include <sys/types.h>

namespace Kernel
{

	class SpinLock
	{
		BAN_NON_COPYABLE(SpinLock);
		BAN_NON_MOVABLE(SpinLock);

	public:
		SpinLock() = default;
		void lock();
		void unlock();
		bool is_locked() const;

		uint32_t lock_depth() const { return m_locker != -1; }

	private:
		BAN::Atomic<pid_t> m_locker = -1;
	};

	class RecursiveSpinLock
	{
		BAN_NON_COPYABLE(RecursiveSpinLock);
		BAN_NON_MOVABLE(RecursiveSpinLock);

	public:
		RecursiveSpinLock() = default;
		void lock();
		void unlock();
		bool is_locked() const;

		uint32_t lock_depth() const { return m_lock_depth; }

	private:
		BAN::Atomic<pid_t>		m_locker		= -1;
		BAN::Atomic<uint32_t>	m_lock_depth	= 0;
	};

	class RecursivePrioritySpinLock
	{
		BAN_NON_COPYABLE(RecursivePrioritySpinLock);
		BAN_NON_MOVABLE(RecursivePrioritySpinLock);

	public:
		RecursivePrioritySpinLock() = default;
		void lock();
		void unlock();
		bool is_locked() const;

		uint32_t lock_depth() const { return m_lock_depth; }

	private:
		BAN::Atomic<pid_t>		m_locker		= -1;
		BAN::Atomic<uint32_t>	m_lock_depth	= 0;
		BAN::Atomic<uint32_t>	m_queue_length	= 0;
	};

}
