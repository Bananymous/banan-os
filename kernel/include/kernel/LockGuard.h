#pragma once

#include <BAN/NoCopyMove.h>

namespace Kernel
{

	template<typename Lock>
	class LockGuard
	{
		BAN_NON_COPYABLE(LockGuard<Lock>);
		BAN_NON_MOVABLE(LockGuard<Lock>);

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