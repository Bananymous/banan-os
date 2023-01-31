#pragma once

#include <BAN/LinkedList.h>
#include <kernel/Thread.h>

namespace Kernel
{

	class Scheduler
	{
		BAN_NON_COPYABLE(Scheduler);
		BAN_NON_MOVABLE(Scheduler);
		
	public:
		static void Initialize();
		static Scheduler& Get();

		const Thread& CurrentThread() const;

		void AddThread(void(*)());
		void Switch();
		void Start();

		static constexpr size_t ms_between_switch = 4;

	private:
		Scheduler() {}

	private:
		BAN::LinkedList<Thread>				m_threads;
		BAN::LinkedList<Thread>::iterator	m_current_iterator;
	};

}