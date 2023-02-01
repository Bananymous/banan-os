#include <kernel/Arch.h>
#include <kernel/InterruptController.h>
#include <kernel/Scheduler.h>

namespace Kernel
{

	static Scheduler* s_instance = nullptr;

	extern "C" uintptr_t read_rip();
	asm(
	".global read_rip;"
	"read_rip:"
#if ARCH(x86_64)
		"popq %rax;"
		"jmp *%rax"
#else
		"popl %eax;"
		"jmp *%eax"
#endif
	);

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

	//void Scheduler::AddThread(const BAN::Function<void()>& function)
	//{
	//	MUST(m_threads.EmplaceBack(function));
	//}

	void Scheduler::switch_thread()
	{
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

		if (current.state() == Thread::State::Done)
		{
			// NOTE: this does not invalidate the next/next_iterator
			//       since we are working with linked list
			m_threads.remove(m_current_iterator);
			m_current_iterator = decltype(m_threads)::iterator();
		}

		uintptr_t rip = read_rip();
		if (rip == 0)
			return;

		uintptr_t rsp;
#if ARCH(x86_64)
		asm volatile("movq %%rsp, %0" : "=r"(rsp));
#else
		asm volatile("movl %%esp, %0" : "=r"(rsp));
#endif

		if (m_current_iterator)
		{
			current.set_rip(rip);
			current.set_rsp(rsp);
			current.set_state(Thread::State::Paused);
		}

		m_current_iterator = next_iterator;

		if (next.state() == Thread::State::NotStarted)
		{
			InterruptController::Get().EOI(PIT_IRQ);
			next.set_state(Thread::State::Running);
			asm volatile(
#if ARCH(x86_64)
				"movq %0, %%rsp;"
#else
				"movl %0, %%esp;"
#endif
				"sti;"
				"jmp *%1;"
				:: "r"(next.rsp()), "r"(next.rip())
			);
		}
		else if (next.state() == Thread::State::Paused)
		{
			next.set_state(Thread::State::Running);
			asm volatile(
#if ARCH(x86_64)
				"movq %0, %%rsp;"
				"movq $0, %%rax;"
#else
				"movl %0, %%esp;"
				"movl $0, %%eax;"
#endif
				"jmp *%1;"
				:: "r"(next.rsp()), "r"(next.rip())
			);
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
		asm volatile(
#if ARCH(x86_64)
			"movq %0, %%rsp;"
#else
			"movl %0, %%esp;"
#endif
			"sti;"
			"jmp *%1;"
			:: "r"(current.rsp()), "r"(current.rip())
		);
	}

}