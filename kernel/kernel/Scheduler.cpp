#include <kernel/Arch.h>
#include <kernel/InterruptController.h>
#include <kernel/Scheduler.h>

namespace Kernel
{

	static Scheduler* s_instance = nullptr;

	extern "C" void start_thread(uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t rsp, uintptr_t rbp, uintptr_t rip);
	extern "C" void continue_thread(uintptr_t rsp, uintptr_t rbp, uintptr_t rip);
	extern "C" uintptr_t read_rip();

	void Scheduler::initialize()
	{
		ASSERT(!s_instance);
		s_instance = new Scheduler();
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

	void Scheduler::switch_thread()
	{
		uintptr_t rsp, rbp, rip;
		if (!(rip = read_rip()))
			return;
		read_rsp(rsp);
		read_rbp(rbp);

		static uint8_t cnt = 0;
		if (cnt++ % ms_between_switch)
			return;

		ASSERT(InterruptController::get().is_in_service(PIT_IRQ));

		ASSERT(m_threads.size() > 0);
		if (m_threads.size() == 1)
			return;

		ASSERT(m_current_iterator);

		auto next_iterator = m_current_iterator;
		if (++next_iterator == m_threads.end())
			next_iterator = m_threads.begin();

		Thread& current = *m_current_iterator;
		Thread& next 	= *next_iterator;

		ASSERT(next.state() == Thread::State::Paused || next.state() == Thread::State::NotStarted);

		if (current.state() == Thread::State::Done)
		{
			// NOTE: this does not invalidate the next/next_iterator
			//       since we are working with linked list
			m_threads.remove(m_current_iterator);
			m_current_iterator = decltype(m_threads)::iterator();
		}

		if (m_current_iterator)
		{
			current.set_rsp(rsp);
			current.set_rbp(rbp);
			current.set_rip(rip);
			current.set_state(Thread::State::Paused);
		}

		m_current_iterator = next_iterator;

		if (next.state() == Thread::State::NotStarted)
		{
			InterruptController::get().eoi(PIT_IRQ);
			next.set_state(Thread::State::Running);
			const uintptr_t* args = next.args();
			start_thread(args[0], args[1], args[2], args[3], next.rsp(), next.rbp(), next.rip());
		}
		else if (next.state() == Thread::State::Paused)
		{
			next.set_state(Thread::State::Running);
			BOCHS_BREAK();
			continue_thread(next.rsp(), next.rbp(), next.rip());
		}
		
		ASSERT(false);
	}

	void Scheduler::start()
	{
		ASSERT(m_threads.size() > 0);

		m_current_iterator = m_threads.begin();

		Thread& current = *m_current_iterator;
		ASSERT(current.state() == Thread::State::NotStarted);
		current.set_state(Thread::State::Running);

		const uintptr_t* args = current.args();
		start_thread(args[0], args[1], args[2], args[3], current.rsp(), current.rbp(), current.rip());

		ASSERT(false);
	}

}