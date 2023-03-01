#include <kernel/Arch.h>
#include <kernel/InterruptController.h>
#include <kernel/Scheduler.h>

namespace Kernel
{

	static Scheduler* s_instance = nullptr;

	extern "C" void start_thread(const BAN::Function<void()>* function, uintptr_t rsp, uintptr_t rip);
	extern "C" void continue_thread(uintptr_t rsp, uintptr_t rip);
	extern "C" uintptr_t read_rip();

	void Scheduler::initialize()
	{
		ASSERT(!s_instance);
		s_instance = new Scheduler();
		ASSERT(s_instance);
		MUST(s_instance->add_thread(BAN::Function<void()>([] { for(;;) asm volatile("hlt"); })));
	}

	Scheduler& Scheduler::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	const Thread& Scheduler::current_thread() const
	{
		return *m_current_iterator;
	}


	BAN::ErrorOr<void> Scheduler::add_thread(const BAN::Function<void()>& function)
	{
		uintptr_t flags;
		asm volatile("pushf; pop %0" : "=r"(flags));
		asm volatile("cli");
		TRY(m_threads.emplace_back(function));
		if (flags & (1 << 9))
			asm volatile("sti");
		return {};
	}

	void Scheduler::reschedule()
	{
		ASSERT(InterruptController::get().is_in_service(PIT_IRQ));
		InterruptController::get().eoi(PIT_IRQ);

		uint64_t current_time = PIT::ms_since_boot();
		if (m_last_reschedule + ms_between_switch > current_time)
			return;
		m_last_reschedule = current_time;

		for (Thread& thread : m_threads)
			if (thread.state() == Thread::State::Sleeping)
				thread.set_state(Thread::State::Paused);

		switch_thread();
	}

	void Scheduler::switch_thread()
	{
		uintptr_t rsp, rip;
		push_callee_saved();
		if (!(rip = read_rip()))
		{
			pop_callee_saved();
			return;
		}
		read_rsp(rsp);

		ASSERT(m_threads.size() > 1);
		
		Thread& current = *m_current_iterator;

		if (current.state() == Thread::State::Done)
		{
			m_threads.remove(m_current_iterator);
			m_current_iterator = m_threads.begin();
		}
		else
		{
			current.set_rsp(rsp);
			current.set_rip(rip);
			if (current.state() != Thread::State::Sleeping)
				current.set_state(Thread::State::Paused);
		}

		auto next_iterator = m_current_iterator;
		if (++next_iterator == m_threads.end())
			next_iterator = ++m_threads.begin();
		if (next_iterator->state() == Thread::State::Sleeping)
			next_iterator = m_threads.begin();
		Thread& next = *next_iterator;

		m_current_iterator = next_iterator;

		switch (next.state())
		{
			case Thread::State::NotStarted:
				next.set_state(Thread::State::Running);
				start_thread(next.function(), next.rsp(), next.rip());
				break;
			case Thread::State::Paused:
				next.set_state(Thread::State::Running);
				continue_thread(next.rsp(), next.rip());
				break;
			case Thread::State::Sleeping:	ASSERT(false);
			case Thread::State::Running:	ASSERT(false);
			case Thread::State::Done:		ASSERT(false);
		}
		
		ASSERT(false);
	}
	
	void Scheduler::set_current_thread_sleeping()
	{
		asm volatile("cli");
		m_current_iterator->set_state(Thread::State::Sleeping);
		switch_thread();
		asm volatile("sti");
	}

	void Scheduler::start()
	{
		ASSERT(m_threads.size() > 1);

		m_current_iterator = m_threads.begin();

		Thread& current = *m_current_iterator;
		ASSERT(current.state() == Thread::State::NotStarted);
		current.set_state(Thread::State::Running);

		start_thread(current.function(), current.rsp(), current.rip());

		ASSERT(false);
	}

}