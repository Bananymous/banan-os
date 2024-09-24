#pragma once

#include <BAN/Math.h>
#include <kernel/Lock/SpinLock.h>
#include <kernel/Scheduler.h>

namespace Kernel
{

	class ThreadBlocker
	{
	public:
		void block_indefinite();
		void block_with_timeout_ms(uint64_t timeout_ms) { ASSERT(!BAN::Math::will_multiplication_overflow<uint64_t>(timeout_ms, 1'000'000)); return block_with_timeout_ns(timeout_ms * 1'000'000); }
		void block_with_wake_time_ms(uint64_t wake_time_ms) { ASSERT(!BAN::Math::will_multiplication_overflow<uint64_t>(wake_time_ms, 1'000'000)); return block_with_wake_time_ns(wake_time_ms * 1'000'000); }
		void block_with_timeout_ns(uint64_t timeout_ns);
		void block_with_wake_time_ns(uint64_t wake_time_ns);
		void unblock();

	private:
		void add_thread_to_block_queue(SchedulerQueue::Node*);
		void remove_blocked_thread(SchedulerQueue::Node*);

	private:
		SpinLock m_lock;
		SchedulerQueue::Node* m_block_chain { nullptr };

		friend class Scheduler;
	};

}
