#pragma once

#include <kernel/ProcessorID.h>
#include <kernel/Lock/SpinLock.h>

namespace Kernel
{

	class Thread;
	class ThreadBlocker;

	struct SchedulerQueueNode
	{
		SchedulerQueueNode(Thread* thread)
			: thread(thread)
		{}

		Thread* const thread;

		SchedulerQueueNode* next { nullptr };
		SchedulerQueueNode* prev { nullptr };

		uint64_t wake_time_ns { static_cast<uint64_t>(-1) };

		SpinLock blocker_lock;
		ThreadBlocker* blocker { nullptr };

		ProcessorID processor_id { PROCESSOR_NONE };
		bool blocked { false };

		uint64_t last_start_ns { 0 };
		uint64_t time_used_ns  { 0 };
	};

}
