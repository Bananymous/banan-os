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

		BAN::ErrorOr<void> add_thread(BAN::RefCounted<Thread>);

		void set_current_thread_sleeping(uint64_t);
		[[noreturn]] void set_current_thread_done();

	private:
		Scheduler() = default;

		BAN::RefCounted<Thread> current_thread();

		void wake_threads();
		[[nodiscard]] bool save_current_thread();
		void get_next_thread();
		[[noreturn]] void execute_current_thread();

	private:
		struct ActiveThread
		{
			BAN::RefCounted<Thread> thread;
			uint64_t padding;
		};

		struct SleepingThread
		{
			BAN::RefCounted<Thread> thread;
			uint64_t wake_delta;
		};

		BAN::RefCounted<Thread> m_idle_thread;
		BAN::LinkedList<ActiveThread> m_active_threads;
		BAN::LinkedList<SleepingThread> m_sleeping_threads;

		BAN::LinkedList<ActiveThread>::iterator m_current_thread;

		uint64_t m_last_reschedule = 0;
	};

}