#pragma once

#include <BAN/LinkedList.h>
#include <kernel/Semaphore.h>
#include <kernel/Thread.h>

namespace Kernel
{

	class Scheduler
	{
	public:
		static BAN::ErrorOr<void> initialize();
		static Scheduler& get();

		[[noreturn]] void start();

		void timer_reschedule();
		void reschedule();
		void reschedule_if_idling();

		void set_current_thread_sleeping(uint64_t);

		void block_current_thread(Semaphore*);
		void unblock_threads(Semaphore*);
		// Makes sleeping or blocked thread with tid active.
		void unblock_thread(pid_t tid);

		Thread& current_thread();
		static pid_t current_tid();

		[[noreturn]] void execute_current_thread();
		[[noreturn]] void _execute_current_thread();
		[[noreturn]] void delete_current_process_and_thread();

	private:
		Scheduler() = default;

		void wake_threads();
		[[nodiscard]] bool save_current_thread();
		void remove_and_advance_current_thread();
		void advance_current_thread();

		BAN::ErrorOr<void> add_thread(Thread*);

	private:
		struct SchedulerThread
		{
			SchedulerThread(Thread* thread)
				: thread(thread)
			{}

			Thread* thread;
			union
			{
				uint64_t	wake_time;
				Semaphore*	semaphore;
			};
		};

		Thread* m_idle_thread { nullptr };
		BAN::LinkedList<SchedulerThread> m_active_threads;
		BAN::LinkedList<SchedulerThread> m_sleeping_threads;
		BAN::LinkedList<SchedulerThread> m_blocking_threads;

		BAN::LinkedList<SchedulerThread>::iterator m_current_thread;

		friend class Process;
	};

}
