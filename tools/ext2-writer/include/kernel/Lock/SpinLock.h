#pragma once

namespace Kernel
{

	class SpinLock
	{
	public:
		void lock()   {}
		void unlock() {}
	};

	class SpinLockGuard
	{
	public:
		SpinLockGuard(SpinLock&) {}
	};

}
