#include <kernel/LockGuard.h>
#include <kernel/Scheduler.h>
#include <kernel/Semaphore.h>

namespace Kernel
{

	void Semaphore::block()
	{
		Scheduler::get().block_current_thread(this);
	}

	void Semaphore::unblock()
	{
		if (!m_blocked)
			return;
		Scheduler::get().unblock_threads(this);
	}

}