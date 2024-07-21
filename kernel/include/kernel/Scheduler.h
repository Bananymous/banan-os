#pragma once

#include <BAN/Array.h>
#include <BAN/ForwardList.h>
#include <BAN/NoCopyMove.h>
#include <kernel/InterruptStack.h>

#include <sys/types.h>

namespace Kernel
{

	class Thread;
	class ThreadBlocker;

	class SchedulerQueue
	{
	public:
		struct Node
		{
			Node(Thread* thread)
				: thread(thread)
			{}

			Node* next { nullptr };
			Node* prev { nullptr };

			Thread* thread;
			ThreadBlocker* blocker { nullptr };
			uint64_t wake_time_ns { static_cast<uint64_t>(-1) };

			uint64_t last_start_ns { 0 };
			uint64_t time_used_ns  { 0 };
		};

	public:
		void add_thread_to_back(Node*);
		void add_thread_with_wake_time(Node*);
		template<typename F>
		Node* remove_with_condition(F callback);
		void remove_node(Node*);
		Node* front();
		Node* pop_front();

		bool empty() const { return m_head == nullptr; }

	private:
		Node* m_head { nullptr };
		Node* m_tail { nullptr };
	};

	class Scheduler
	{
		BAN_NON_COPYABLE(Scheduler);
		BAN_NON_MOVABLE(Scheduler);

	public:
		struct NewThreadRequest
		{
			SchedulerQueue::Node* node;
			bool blocked;
		};

		struct UnblockRequest
		{
			enum class Type
			{
				ThreadBlocker,
				ThreadID,
			};
			Type type;
			union
			{
				ThreadBlocker* blocker;
				pid_t tid;
			};
		};

	public:
		static BAN::ErrorOr<Scheduler*> create();
		BAN::ErrorOr<void> initialize();

		void reschedule(InterruptStack*, InterruptRegisters*);
		void reschedule_if_idle();

		void timer_interrupt();

		BAN::ErrorOr<void> add_thread(Thread*);

		void block_current_thread(ThreadBlocker* thread_blocker, uint64_t wake_time_ns);
		void unblock_threads(ThreadBlocker*);
		void unblock_thread(pid_t tid);

		Thread& current_thread();
		Thread& idle_thread();

		pid_t current_tid() const;
		bool is_idle() const;

	private:
		Scheduler() = default;

		void add_current_to_most_loaded(SchedulerQueue* target_queue);
		void update_most_loaded_node_queue(SchedulerQueue::Node*, SchedulerQueue* target_queue);
		void remove_node_from_most_loaded(SchedulerQueue::Node*);

		bool do_unblock(ThreadBlocker*);
		bool do_unblock(pid_t);
		void do_load_balancing();

		class ProcessorID find_least_loaded_processor() const;

		void preempt();

		void handle_unblock_request(const UnblockRequest&);
		void handle_new_thread_request(const NewThreadRequest&);

	private:
		SchedulerQueue m_run_queue;
		SchedulerQueue m_block_queue;
		SchedulerQueue::Node* m_current { nullptr };
		bool m_current_will_block { false };

		uint32_t m_thread_count { 0 };

		InterruptStack* m_interrupt_stack { nullptr };
		InterruptRegisters* m_interrupt_registers { nullptr };

		uint64_t m_last_reschedule_ns { 0 };
		uint64_t m_last_load_balance_ns { 0 };

		struct ThreadInfo
		{
			SchedulerQueue*       queue { nullptr };
			SchedulerQueue::Node* node  { nullptr };
		};
		BAN::Array<ThreadInfo, 10> m_most_loaded_threads;

		uint64_t m_idle_start_ns { 0 };
		uint64_t m_idle_ns { 0 };

		bool m_should_calculate_max_load_threads { true };

		Thread* m_idle_thread { nullptr };

		friend class Processor;
	};

}
