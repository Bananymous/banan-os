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

		void set_current_thread_sleeping(uint64_t wake_time);

		void block_current_thread(Semaphore*, uint64_t wake_time);
		void unblock_threads(Semaphore*);
		// Makes sleeping or blocked thread with tid active.
		void unblock_thread(pid_t tid);

		Thread& current_thread();
		static pid_t current_tid();

		[[noreturn]] void execute_current_thread();
		[[noreturn]] void execute_current_thread_locked();
		[[noreturn]] void delete_current_process_and_thread();

		// This is no return if called on current thread
		void terminate_thread(Thread*);

	private:
		Scheduler() = default;

		void set_current_thread_sleeping_impl(uint64_t wake_time);

		void wake_threads();
		[[nodiscard]] bool save_current_thread();
		void remove_and_advance_current_thread();
		void advance_current_thread();

		[[noreturn]] void execute_current_thread_stack_loaded();

		BAN::ErrorOr<void> add_thread(Thread*);

	private:
		struct SchedulerThread
		{
			SchedulerThread(Thread* thread)
				: thread(thread)
			{}

			Thread*		thread;
			uint64_t	wake_time;
			Semaphore*	semaphore;
		};

		SpinLock m_lock;

		Thread* m_idle_thread { nullptr };
		BAN::LinkedList<SchedulerThread> m_active_threads;
		BAN::LinkedList<SchedulerThread> m_sleeping_threads;

		BAN::LinkedList<SchedulerThread>::iterator m_current_thread;

		friend class Process;
	};

}
