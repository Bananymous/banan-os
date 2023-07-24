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
		Scheduler::get().unblock_threads(this);
	}

}