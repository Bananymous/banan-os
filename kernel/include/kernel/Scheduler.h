#pragma once

#include <BAN/LinkedList.h>
#include <BAN/Memory.h>
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

		BAN::ErrorOr<void> add_thread(BAN::RefPtr<Thread>);

		void set_current_thread_sleeping(uint64_t);
		[[noreturn]] void set_current_thread_done();

		BAN::RefPtr<Thread> current_thread();

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
			ActiveThread(const BAN::RefPtr<Thread>& thread) : thread(thread) {}
			BAN::RefPtr<Thread> thread;
			uint64_t padding = 0;
		};

		struct SleepingThread
		{
			SleepingThread(const BAN::RefPtr<Thread>& thread, uint64_t wake_time) : thread(thread), wake_time(wake_time) {}
			BAN::RefPtr<Thread> thread;
			uint64_t wake_time;
		};

		BAN::RefPtr<Thread> m_idle_thread;
		BAN::LinkedList<ActiveThread> m_active_threads;
		BAN::LinkedList<SleepingThread> m_sleeping_threads;

		BAN::LinkedList<ActiveThread>::iterator m_current_thread;

		uint64_t m_last_reschedule = 0;
	};

}