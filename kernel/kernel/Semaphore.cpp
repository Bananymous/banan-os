#include <kernel/Scheduler.h>
#include <kernel/Semaphore.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	void Semaphore::block_indefinite()
	{
		Scheduler::get().block_current_thread(this, ~(uint64_t)0);
	}

	void Semaphore::block_with_timeout(uint64_t timeout_ms)
	{
		Scheduler::get().block_current_thread(this, SystemTimer::get().ms_since_boot() + timeout_ms);
	}

	void Semaphore::block_with_wake_time(uint64_t wake_time)
	{
		Scheduler::get().block_current_thread(this, wake_time);
	}

	void Semaphore::unblock()
	{
		Scheduler::get().unblock_threads(this);
	}

}
