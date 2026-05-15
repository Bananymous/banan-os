#pragma once

#include <BAN/NoCopyMove.h>

#include <stdint.h>

namespace Kernel
{

	template<typename Lock> requires requires(Lock& lock) { lock.lock(); lock.unlock(); }
	class LockGuard
	{
		BAN_NON_COPYABLE(LockGuard);
		BAN_NON_MOVABLE(LockGuard);

	public:
		LockGuard(Lock& lock)
			: m_lock(lock)
		{
			m_lock.lock();
		}

		~LockGuard()
		{
			m_lock.unlock();
		}

	private:
		Lock& m_lock;
	};

}
