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
		bool try_lock();
		void unlock();

		pid_t locker() const { return m_locker; }
		bool is_locked() const { return m_locker != -1; }
		uint32_t lock_depth() const { return is_locked(); }

	private:
		BAN::Atomic<pid_t> m_locker { -1 };
		uintptr_t m_flags { 0 };
	};

	class RecursiveSpinLock
	{
		BAN_NON_COPYABLE(RecursiveSpinLock);
		BAN_NON_MOVABLE(RecursiveSpinLock);

	public:
		RecursiveSpinLock() = default;

		void lock();
		bool try_lock();
		void unlock();

		pid_t locker() const { return m_locker; }
		bool is_locked() const { return m_locker != -1; }
		uint32_t lock_depth() const { return m_lock_depth; }

	private:
		BAN::Atomic<pid_t> m_locker { -1 };
		uint32_t m_lock_depth { 0 };
		uintptr_t m_flags { 0 };
	};

}
