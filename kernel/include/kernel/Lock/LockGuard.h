#pragma once

#include <BAN/NoCopyMove.h>

#include <stdint.h>

namespace Kernel
{

	template<typename Lock>
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

	template<typename Lock>
	class LockFreeGuard
	{
		BAN_NON_COPYABLE(LockFreeGuard);
		BAN_NON_MOVABLE(LockFreeGuard);

	public:
		LockFreeGuard(Lock& lock)
			: m_lock(lock)
			, m_depth(lock.lock_depth())
		{
			for (uint32_t i = 0; i < m_depth; i++)
				m_lock.unlock();
		}

		~LockFreeGuard()
		{
			for (uint32_t i = 0; i < m_depth; i++)
				m_lock.lock();
		}

	private:
		Lock& m_lock;
		const uint32_t m_depth;
	};

}
