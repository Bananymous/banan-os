#pragma once

namespace Kernel
{

	class RWLock
	{
	public:
		void rd_lock()   {}
		void rd_unlock() {}
		void wr_lock()   {}
		void wr_unlock() {}
	};

	class RWLockRDGuard
	{
		public:
			RWLockRDGuard(RWLock&) {}
	};

	class RWLockWRGuard
	{
		public:
			RWLockWRGuard(RWLock&) {}
	};

}
