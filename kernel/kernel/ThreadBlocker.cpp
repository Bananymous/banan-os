#include <kernel/Processor.h>
#include <kernel/ThreadBlocker.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	void ThreadBlocker::block_indefinite()
	{
		Processor::scheduler().block_current_thread(this, ~static_cast<uint64_t>(0));
	}

	void ThreadBlocker::block_with_timeout_ns(uint64_t timeout_ns)
	{
		Processor::scheduler().block_current_thread(this, SystemTimer::get().ns_since_boot() + timeout_ns);
	}

	void ThreadBlocker::block_with_wake_time_ns(uint64_t wake_time_ns)
	{
		Processor::scheduler().block_current_thread(this, wake_time_ns);
	}

	void ThreadBlocker::unblock()
	{
		Processor::scheduler().unblock_threads(this);
	}

}
