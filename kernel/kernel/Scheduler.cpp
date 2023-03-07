#include <kernel/Arch.h>
#include <kernel/Attributes.h>
#include <kernel/InterruptController.h>
#include <kernel/Scheduler.h>

#include <kernel/PCI.h>

#define DISABLE_INTERRUPTS() asm volatile("cli")
#define ENABLE_INTERRUPTS() asm volatile("sti")

#if 1
	#define VERIFY_CLI() ASSERT(interrupts_disabled())
#else
	#define VERIFY_CLI()
#endif

namespace Kernel
{

	extern "C" void start_thread(const BAN::Function<void()>* function, uintptr_t rsp, uintptr_t rip);
	extern "C" void continue_thread(uintptr_t rsp, uintptr_t rip);
	extern "C" uintptr_t read_rip();

	static Scheduler* s_instance = nullptr;

	static bool interrupts_disabled()
	{
		uintptr_t flags;
		asm volatile("pushf; pop %0" : "=r"(flags));
		return !(flags & (1 << 9));
	}

	BAN::ErrorOr<void> Scheduler::initialize()
	{
		ASSERT(s_instance == nullptr);
		s_instance = new Scheduler();
		ASSERT(s_instance);
		s_instance->m_idle_thread = TRY(Thread::create([] { for (;;) asm volatile("hlt"); }));
		return {};
	}

	Scheduler& Scheduler::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	void Scheduler::start()
	{
		VERIFY_CLI();
		ASSERT(!m_active_threads.empty());
		m_current_thread = m_active_threads.begin();
		execute_current_thread();
		ASSERT_NOT_REACHED();
	}

	BAN::RefCounted<Thread> Scheduler::current_thread()
	{
		return m_current_thread ? m_current_thread->thread : m_idle_thread;
	}

	void Scheduler::reschedule()
	{
		ASSERT(InterruptController::get().is_in_service(PIT_IRQ));
		InterruptController::get().eoi(PIT_IRQ);

		if (PIT::ms_since_boot() <= m_last_reschedule)
			return;
		m_last_reschedule = PIT::ms_since_boot();
		
		if (!m_sleeping_threads.empty())
			m_sleeping_threads.front().wake_delta--;
		wake_threads();

		if (save_current_thread())
			return;
		get_next_thread();
		execute_current_thread();
		ASSERT_NOT_REACHED();
	}

	void Scheduler::wake_threads()
	{
		VERIFY_CLI();

		while (!m_sleeping_threads.empty() && m_sleeping_threads.front().wake_delta == 0)
		{
			auto thread = m_sleeping_threads.front().thread;
			m_sleeping_threads.remove(m_sleeping_threads.begin());

			// This should work as we released enough memory from sleeping thread
			static_assert(sizeof(ActiveThread) == sizeof(SleepingThread));
			MUST(m_active_threads.push_back({ thread, 0 }));
		}
	}

	BAN::ErrorOr<void> Scheduler::add_thread(BAN::RefCounted<Thread> thread)
	{
		if (interrupts_disabled())
		{
			TRY(m_active_threads.push_back({ thread, 0 }));
		}
		else
		{
			DISABLE_INTERRUPTS();
			TRY(m_active_threads.push_back({ thread, 0 }));
			ENABLE_INTERRUPTS();
		}
		return {};
	}

	void Scheduler::get_next_thread()
	{
		VERIFY_CLI();

		if (m_active_threads.empty())
		{
			m_current_thread = {};
			return;
		}

		if (!m_current_thread || ++m_current_thread == m_active_threads.end())
			m_current_thread = m_active_threads.begin();
	}

	// NOTE: this is declared always inline, so we don't corrupt the stack
	//       after getting the rsp
	ALWAYS_INLINE bool Scheduler::save_current_thread()
	{
		VERIFY_CLI();

		uintptr_t rsp, rip;
		push_callee_saved();
		if (!(rip = read_rip()))
		{
			pop_callee_saved();
			return true;
		}
		read_rsp(rsp);

		auto current = current_thread();
		current->set_rip(rip);
		current->set_rsp(rsp);
		return false;
	}

	void Scheduler::execute_current_thread()
	{
		VERIFY_CLI();

		auto current = current_thread();

		if (current->started())
		{
			continue_thread(current->rsp(), current->rip());
		}
		else
		{
			current->set_started();
			start_thread(current->function(), current->rsp(), current->rip());
		}

		ASSERT_NOT_REACHED();
	}

	void Scheduler::set_current_thread_sleeping(uint64_t wake_delta)
	{
		DISABLE_INTERRUPTS();

		ASSERT(m_current_thread);

		auto current = m_current_thread->thread;

		auto temp = m_current_thread;
		if (save_current_thread())
			return;
		get_next_thread();
		m_active_threads.remove(temp);

		auto it = m_sleeping_threads.begin();

		for (; it != m_sleeping_threads.end(); it++)
		{
			if (wake_delta <= it->wake_delta)
				break;
			wake_delta -= it->wake_delta;
		}

		if (it != m_sleeping_threads.end())
			it->wake_delta -= wake_delta;

		// This should work as we released enough memory from active thread
		static_assert(sizeof(ActiveThread) == sizeof(SleepingThread));
		MUST(m_sleeping_threads.insert(it, { current, wake_delta }));

		execute_current_thread();
		ASSERT_NOT_REACHED();
	}

	void Scheduler::set_current_thread_done()
	{
		DISABLE_INTERRUPTS();

		ASSERT(m_current_thread);

		auto temp = m_current_thread;
		get_next_thread();
		m_active_threads.remove(temp);
	
		execute_current_thread();
		ASSERT_NOT_REACHED();
	}

}