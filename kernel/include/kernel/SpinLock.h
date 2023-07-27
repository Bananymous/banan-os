#pragma once

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

	private:
		volatile pid_t m_locker = -1;
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

	private:
		pid_t m_locker = -1;
		uint32_t m_lock_depth = 0;
		SpinLock m_lock;
	};

}