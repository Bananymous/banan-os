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
		void add_thread(const BAN::Function<void(Args...)>& func, Args... args)
		{
			uintptr_t flags;
			asm volatile("pushf; pop %0" : "=r"(flags));
			asm volatile("cli");
			MUST(m_threads.emplace_back(func, BAN::forward<Args>(args)...));
			if (flags & (1 << 9))
				asm volatile("sti");
		}

		void switch_thread();
		void start();

		static constexpr size_t ms_between_switch = 1;

	private:
		Scheduler() {}

	private:
		BAN::LinkedList<Thread>				m_threads;
		BAN::LinkedList<Thread>::iterator	m_current_iterator;
	};

}