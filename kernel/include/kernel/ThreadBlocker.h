#pragma once

#include <BAN/Math.h>
#include <kernel/Lock/Mutex.h>
#include <kernel/Lock/SpinLock.h>

namespace Kernel
{

	class SchedulerThreadNode;

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
		void add_thread_to_block_queue(SchedulerThreadNode*);
		void remove_thread_from_block_queue(SchedulerThreadNode*);

	private:
		SchedulerThreadNode* m_block_chain { nullptr };
		SpinLock m_lock;

		friend class Scheduler;
	};

}
