#pragma once

#include <BAN/Atomic.h>
#include <BAN/NoCopyMove.h>
#include <kernel/ProcessorID.h>

namespace Kernel
{

	class Thread;
	class ThreadBlocker;

	struct SchedulerThreadNode
	{
		SchedulerThreadNode(Thread* thread)
			: thread(thread)
			, heap({ nullptr, nullptr, nullptr })
		{}

		Thread* const thread;

		union
		{
			struct
			{
				SchedulerThreadNode* next;
				SchedulerThreadNode* prev;
			} queue;
			struct
			{
				SchedulerThreadNode* parent;
				SchedulerThreadNode* lchild;
				SchedulerThreadNode* rchild;
			} heap;
		};

		uint64_t wake_time_ns { static_cast<uint64_t>(-1) };

		BAN::Atomic<ThreadBlocker*> blocker { nullptr };
		SchedulerThreadNode* block_chain_prev { nullptr };
		SchedulerThreadNode* block_chain_next { nullptr };

		ProcessorID processor_id { PROCESSOR_NONE };
		bool blocked { false };

		uint64_t last_start_ns { 0 };
		uint64_t time_used_ns  { 0 };
	};

	class SchedulerQueue
	{
		BAN_NON_COPYABLE(SchedulerQueue);
		BAN_NON_MOVABLE(SchedulerQueue);
	public:
		SchedulerQueue() = default;

		SchedulerThreadNode* front();
		SchedulerThreadNode* pop_front();

		void push(SchedulerThreadNode*);
		void pop(SchedulerThreadNode*);

		void walk(void (*)(const SchedulerThreadNode*, void*), void*) const;
		bool empty() const { return m_head == nullptr; }

	private:
		SchedulerThreadNode* m_head { nullptr };
		SchedulerThreadNode* m_tail { nullptr };
	};

	class SchedulerHeap
	{
		BAN_NON_COPYABLE(SchedulerHeap);
		BAN_NON_MOVABLE(SchedulerHeap);
	public:
		SchedulerHeap() = default;

		SchedulerThreadNode* front();
		SchedulerThreadNode* pop_front();

		void push(SchedulerThreadNode*);
		void pop(SchedulerThreadNode*);

		void walk(void (*)(const SchedulerThreadNode*, void*), void*) const;
		bool empty() const { return m_root == nullptr; }

	private:
		void walk_impl(void (*)(const SchedulerThreadNode*, void*), void*, const SchedulerThreadNode*) const;
		void swap_nodes(SchedulerThreadNode*, SchedulerThreadNode*);

	private:
		SchedulerThreadNode* m_root { nullptr };
		SchedulerThreadNode* m_last { nullptr };
	};

}
