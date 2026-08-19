#pragma once

#include <BAN/Array.h>
#include <BAN/ForwardList.h>
#include <BAN/NoCopyMove.h>
#include <kernel/InterruptStack.h>
#include <kernel/ProcessorID.h>
#include <kernel/SchedulerThreadNode.h>

#include <sys/types.h>

namespace Kernel
{

	class BaseMutex;
	class Thread;
	class ThreadBlocker;
	class Scheduler
	{
		BAN_NON_COPYABLE(Scheduler);
		BAN_NON_MOVABLE(Scheduler);

	public:
		struct NewThreadRequest
		{
			SchedulerThreadNode* node;
		};

		struct UnblockRequest
		{
			SchedulerThreadNode* node;
		};

	public:
		static BAN::ErrorOr<Scheduler*> create();
		BAN::ErrorOr<void> initialize();

		void reschedule(YieldRegisters*);
		void reschedule_if_needed();

		void on_timer_interrupt();
		void on_yield(YieldRegisters*);

		static BAN::ErrorOr<void> bind_thread_to_processor(Thread*, ProcessorID);
		// if thread is already bound, this will never fail
		BAN::ErrorOr<void> add_thread(Thread*);

		void block_current_thread(ThreadBlocker* thread_blocker, uint64_t wake_time_ns, BaseMutex* mutex);
		void unblock_thread(Thread*);

		Thread& current_thread();
		Thread& idle_thread();

		pid_t current_tid() const;
		bool is_idle() const;

	private:
		Scheduler() = default;

		void add_current_to_most_loaded(void* target_list);
		void update_most_loaded_node_list(SchedulerThreadNode*, void* target_list);
		void remove_node_from_most_loaded(SchedulerThreadNode*);

		void update_wake_up_deadline();
		void wake_up_sleeping_threads();

		void do_load_balancing();

		class ProcessorID find_least_loaded_processor() const;

		void add_thread(SchedulerThreadNode*);
		void unblock_thread(SchedulerThreadNode*);

	private:
		SchedulerQueue m_run_list;
		SchedulerHeap m_block_list;
		SchedulerThreadNode* m_current { nullptr };

		uint32_t m_thread_count { 0 };

		InterruptStack* m_interrupt_stack { nullptr };
		InterruptRegisters* m_interrupt_registers { nullptr };

		uint64_t m_next_reschedule_ns { 0 };
		uint64_t m_last_load_balance_ns { 0 };

		struct ThreadInfo
		{
			void*                list { nullptr };
			SchedulerThreadNode* node { nullptr };
		};
		BAN::Array<ThreadInfo, 10> m_most_loaded_threads;

		uint64_t m_idle_start_ns { 0 };
		uint64_t m_idle_ns { 0 };

		bool m_should_calculate_max_load_threads { true };

		bool m_has_pending_reschedule { false };

		Thread* m_idle_thread { nullptr };

		friend class ThreadBlocker;
		friend class Processor;
	};

}
