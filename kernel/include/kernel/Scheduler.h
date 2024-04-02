#pragma once

#include <kernel/SchedulerQueue.h>
#include <kernel/Semaphore.h>
#include <kernel/Thread.h>

namespace Kernel
{

	class Scheduler
	{
	public:
		static BAN::ErrorOr<void> initialize();
		static Scheduler& get();
		static bool is_started();

		[[noreturn]] void start();

		void yield();

		void timer_reschedule();
		void irq_reschedule();
		void reschedule_if_idling();

		void set_current_thread_sleeping(uint64_t wake_time);

		void block_current_thread(Semaphore*, uint64_t wake_time);
		void unblock_threads(Semaphore*);
		// Makes sleeping or blocked thread with tid active.
		void unblock_thread(pid_t tid);

		Thread& current_thread();
		static pid_t current_tid();

		// This is no return if called on current thread
		void terminate_thread(Thread*);

	private:
		Scheduler() = default;

		void set_current_thread_sleeping_impl(Semaphore* semaphore, uint64_t wake_time);

		void setup_next_thread();

		BAN::ErrorOr<void> add_thread(Thread*);

	private:
		SpinLock m_lock;

		SchedulerQueue m_active_threads;
		SchedulerQueue m_blocking_threads;

		friend class Process;
	};

}
