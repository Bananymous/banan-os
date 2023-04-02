#pragma once

#include <BAN/LinkedList.h>
#include <BAN/Memory.h>
#include <kernel/Semaphore.h>
#include <kernel/Thread.h>

namespace Kernel
{

	class Scheduler
	{
	public:
		static BAN::ErrorOr<void> initialize();
		static Scheduler& get();

		void start();
		void reschedule();
		void reschedule_if_idling();

		BAN::ErrorOr<void> add_thread(Thread*);

		void set_current_thread_sleeping(uint64_t);
		[[noreturn]] void set_current_thread_done();

		void block_current_thread(Semaphore*);
		void unblock_threads(Semaphore*);

		Thread& current_thread();

	private:
		Scheduler() = default;

		void wake_threads();
		[[nodiscard]] bool save_current_thread();
		void remove_and_advance_current_thread();
		void advance_current_thread();
		[[noreturn]] void execute_current_thread();

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

		uint64_t m_last_reschedule = 0;
	};

}