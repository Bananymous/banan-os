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

		template<typename... Args>
		[[nodiscard]] BAN::ErrorOr<void> add_thread(const BAN::Function<void(Args...)>& func, Args... args)
		{
			uintptr_t flags;
			asm volatile("pushf; pop %0" : "=r"(flags));
			asm volatile("cli");
			TRY(m_threads.emplace_back(func, BAN::forward<Args>(args)...));
			if (flags & (1 << 9))
				asm volatile("sti");
			return {};
		}

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