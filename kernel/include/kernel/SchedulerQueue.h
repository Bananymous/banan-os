#pragma once

#include <BAN/Assert.h>
#include <BAN/NoCopyMove.h>

#include <stdint.h>

namespace Kernel
{

	class Thread;
	class Semaphore;

	class SchedulerQueue
	{
		BAN_NON_COPYABLE(SchedulerQueue);
		BAN_NON_MOVABLE(SchedulerQueue);

	public:
		struct Node
		{
			Node(Thread* thread)
				: thread(thread)
			{}

			Thread*		thread;
			uint64_t	wake_time { 0 };
			Semaphore*	semaphore { nullptr };
			bool		should_block { false };

		private:
			Node* next { nullptr };
			friend class SchedulerQueue;
			friend class Scheduler;
		};

	public:
		SchedulerQueue() = default;
		~SchedulerQueue() { ASSERT_NOT_REACHED(); }

		bool empty() const { return m_front == nullptr; }

		Node* pop_front()
		{
			ASSERT(!empty());

			Node* node = m_front;

			m_front = m_front->next;
			if (m_front == nullptr)
				m_back = nullptr;

			node->next = nullptr;

			return node;
		}

		void push_back(Node* node)
		{
			ASSERT(node);
			node->next = nullptr;

			(empty() ? m_front : m_back->next) = node;
			m_back = node;
		}

		void add_with_wake_time(Node* node)
		{
			ASSERT(node);
			node->next = nullptr;

			if (empty() || node->wake_time >= m_back->wake_time)
			{
				push_back(node);
				return;
			}

			if (node->wake_time < m_front->wake_time)
			{
				node->next = m_front;
				m_front = node;
				return;
			}

			Node* prev = m_front;
			for (; node->wake_time >= prev->next->wake_time; prev = prev->next)
				continue;
			node->next = prev->next;
			prev->next = node;
		}

		void remove_with_wake_time(SchedulerQueue& out, uint64_t current_time)
		{
			while (!empty() && m_front->wake_time <= current_time)
				out.push_back(pop_front());
		}

		template<typename F>
		void remove_with_condition(SchedulerQueue& out, F comp)
		{
			while (!empty() && comp(m_front))
				out.push_back(pop_front());

			if (empty())
				return;

			for (Node* prev = m_front; prev->next;)
			{
				Node* node = prev->next;
				if (!comp(node))
					prev = prev->next;
				else
				{
					prev->next = node->next;
					if (node == m_back)
						m_back = prev;
					out.push_back(node);
				}
			}
		}

	private:
		Node* m_front { nullptr };
		Node* m_back { nullptr };
	};

}
