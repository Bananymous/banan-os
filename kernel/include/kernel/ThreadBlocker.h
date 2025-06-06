#pragma once

#include <BAN/Math.h>
#include <kernel/Lock/SpinLock.h>
#include <kernel/Scheduler.h>

namespace Kernel
{

	class ThreadBlocker
	{
	public:
		void block_indefinite(BaseMutex*);
		void block_with_timeout_ns(uint64_t timeout_ns, BaseMutex*);
		void block_with_wake_time_ns(uint64_t wake_time_ns, BaseMutex*);
		void unblock();


		void block_with_timeout_ms(uint64_t timeout_ms, BaseMutex* mutex)
		{
			ASSERT(!BAN::Math::will_multiplication_overflow<uint64_t>(timeout_ms, 1'000'000));
			return block_with_timeout_ns(timeout_ms * 1'000'000, mutex);
		}
		void block_with_wake_time_ms(uint64_t wake_time_ms, BaseMutex* mutex)
		{
			ASSERT(!BAN::Math::will_multiplication_overflow<uint64_t>(wake_time_ms, 1'000'000));
			return block_with_wake_time_ns(wake_time_ms * 1'000'000, mutex);
		}

	private:
		void add_thread_to_block_queue(SchedulerQueue::Node*);
		void remove_blocked_thread(SchedulerQueue::Node*);

	private:
		SpinLock m_lock;
		SchedulerQueue::Node* m_block_chain { nullptr };

		friend class Scheduler;
	};

}
