#pragma once

#include <BAN/NoCopyMove.h>

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
	
	private:
		int m_lock = 0;
	};

}