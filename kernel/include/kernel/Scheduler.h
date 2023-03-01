#pragma once

#include <BAN/Function.h>
#include <BAN/LinkedList.h>
#include <kernel/Thread.h>

namespace Kernel
{

	class Scheduler
	{
		BAN_NON_COPYABLE(Scheduler);
		BAN_NON_MOVABLE(Scheduler);
		
	public:
		static void initialize();
		static Scheduler& get();

		const Thread& current_thread() const;
		
		BAN::ErrorOr<void> add_thread(const BAN::Function<void()>& function);

		void reschedule();
		void set_current_thread_sleeping();
		void start();

		static constexpr size_t ms_between_switch = 4;

	private:
		Scheduler() {}
		void switch_thread();

	private:
		BAN::LinkedList<Thread>				m_threads;
		BAN::LinkedList<Thread>::iterator	m_current_iterator;
		uint64_t							m_last_reschedule = 0;

		friend class Thread;
	};

}