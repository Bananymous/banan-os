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
		struct ActiveThread
		{
			ActiveThread(Thread* thread) : thread(thread) {}
			Thread* thread;
			uint64_t padding;
		};

		struct SleepingThread
		{
			SleepingThread(Thread* thread, uint64_t wake_time) : thread(thread), wake_time(wake_time) {}
			Thread* thread;
			uint64_t wake_time;
		};

		struct BlockingThread
		{
			BlockingThread(Thread* thread, Semaphore* semaphore) : thread(thread), semaphore(semaphore) {}
			Thread* thread;
			Semaphore* semaphore;
			uint8_t padding[sizeof(uint64_t) - sizeof(Semaphore*)];
		};

		Thread* m_idle_thread { nullptr };
		BAN::LinkedList<ActiveThread> m_active_threads;
		BAN::LinkedList<SleepingThread> m_sleeping_threads;
		BAN::LinkedList<BlockingThread> m_blocking_threads;

		BAN::LinkedList<ActiveThread>::iterator m_current_thread;

		friend class Process;
	};

}