#pragma once

#include <BAN/LinkedList.h>
#include <kernel/Semaphore.h>
#include <kernel/Thread.h>

namespace Kernel
{

	class SchedulerLock
	{
	public:
		void lock();
		void unlock();
		void unlock_all();
		pid_t locker() const;

	private:
		BAN::Atomic<pid_t> m_locker { -1 };
		uint32_t m_lock_depth { 0 };

		friend class Scheduler;
	};

	class Scheduler
	{
	public:
		static BAN::ErrorOr<void> initialize();
		static Scheduler& get();

		[[noreturn]] void start();

		void timer_reschedule();
		void reschedule();
		void reschedule_if_idling();

		void reschedule_current_no_save();

		void set_current_thread_sleeping(uint64_t wake_time);

		void block_current_thread(Semaphore*, uint64_t wake_time);
		void unblock_threads(Semaphore*);
		// Makes sleeping or blocked thread with tid active.
		void unblock_thread(pid_t tid);

		Thread& current_thread();
		static pid_t current_tid();

		BAN::ErrorOr<void> add_thread(Thread*);

		[[noreturn]] void delete_current_process_and_thread();

	private:
		Scheduler() = default;

		void set_current_thread_sleeping_impl(uint64_t wake_time);

		void wake_threads();
		[[nodiscard]] bool save_current_thread();
		void remove_and_advance_current_thread();
		void advance_current_thread();

		[[noreturn]] void execute_current_thread();
		[[noreturn]] void _execute_current_thread();

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

		SchedulerLock m_lock;

		Thread* m_idle_thread { nullptr };
		BAN::LinkedList<SchedulerThread> m_active_threads;
		BAN::LinkedList<SchedulerThread> m_sleeping_threads;

		BAN::LinkedList<SchedulerThread>::iterator m_current_thread;
	};

}
