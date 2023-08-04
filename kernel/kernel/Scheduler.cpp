#include <kernel/Arch.h>
#include <kernel/Attributes.h>
#include <kernel/CriticalScope.h>
#include <kernel/GDT.h>
#include <kernel/InterruptController.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Timer/PIT.h>

#if 1
	#define VERIFY_STI() ASSERT(interrupts_enabled())
	#define VERIFY_CLI() ASSERT(!interrupts_enabled())
#else
	#define VERIFY_STI()
	#define VERIFY_CLI()
#endif

namespace Kernel
{

	extern "C" [[noreturn]] void start_thread(uintptr_t rsp, uintptr_t rip);
	extern "C" [[noreturn]] void continue_thread(uintptr_t rsp, uintptr_t rip);

	static Scheduler* s_instance = nullptr;

	static uint8_t s_temp_stack[1024];
	ALWAYS_INLINE static void load_temp_stack()
	{
		asm volatile("movq %0, %%rsp" :: "r"(s_temp_stack + sizeof(s_temp_stack)));
	}

	BAN::ErrorOr<void> Scheduler::initialize()
	{
		ASSERT(s_instance == nullptr);
		s_instance = new Scheduler();
		ASSERT(s_instance);
		s_instance->m_idle_thread = TRY(Thread::create_kernel([](void*) { for (;;) asm volatile("hlt"); }, nullptr, nullptr));
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

	Thread& Scheduler::current_thread()
	{
		return m_current_thread ? *m_current_thread->thread : *m_idle_thread;
	}

	pid_t Scheduler::current_tid()
	{
		if (s_instance == nullptr)
			return 0;
		return Scheduler::get().current_thread().tid();
	}

	void Scheduler::timer_reschedule()
	{
		VERIFY_CLI();

		ASSERT(InterruptController::get().is_in_service(PIT_IRQ));
		InterruptController::get().eoi(PIT_IRQ);

		wake_threads();

		if (save_current_thread())
			return;
		advance_current_thread();
		execute_current_thread();
		ASSERT_NOT_REACHED();
	}

	void Scheduler::reschedule()
	{
		DISABLE_INTERRUPTS();

		if (save_current_thread())
		{
			ENABLE_INTERRUPTS();
			return;
		}
		advance_current_thread();
		execute_current_thread();
		ASSERT_NOT_REACHED();
	}

	void Scheduler::reschedule_if_idling()
	{
		VERIFY_CLI();

		if (m_active_threads.empty() || &current_thread() != m_idle_thread)
			return;
		
		if (save_current_thread())
			return;
		m_current_thread = m_active_threads.begin();
		execute_current_thread();
		ASSERT_NOT_REACHED();
	}

	void Scheduler::wake_threads()
	{
		VERIFY_CLI();

		uint64_t current_time = TimerHandler::get().ms_since_boot();
		while (!m_sleeping_threads.empty() && m_sleeping_threads.front().wake_time <= current_time)
		{
			Thread* thread = m_sleeping_threads.front().thread;
			m_sleeping_threads.remove(m_sleeping_threads.begin());

			// This should work as we released enough memory from sleeping thread
			static_assert(sizeof(ActiveThread) == sizeof(SleepingThread));
			MUST(m_active_threads.emplace_back(thread));
		}
	}

	BAN::ErrorOr<void> Scheduler::add_thread(Thread* thread)
	{
		CriticalScope _;
		TRY(m_active_threads.emplace_back(thread));
		return {};
	}

	void Scheduler::advance_current_thread()
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

	void Scheduler::remove_and_advance_current_thread()
	{
		VERIFY_CLI();

		ASSERT(m_current_thread);

		if (m_active_threads.size() == 1)
		{
			m_active_threads.remove(m_current_thread);
			m_current_thread = {};
		}
		else
		{
			auto temp = m_current_thread;
			advance_current_thread();
			m_active_threads.remove(temp);
		}
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

		Thread& current = current_thread();
		current.set_rip(rip);
		current.set_rsp(rsp);

		load_temp_stack();

		return false;
	}

	void Scheduler::delete_current_process_and_thread()
	{
		DISABLE_INTERRUPTS();

		load_temp_stack();
		PageTable::kernel().load();

		Thread* thread = m_current_thread->thread;

		ASSERT(thread->has_process());
		delete &thread->process();
		
		remove_and_advance_current_thread();

		delete thread;

		execute_current_thread();
	}

	void Scheduler::execute_current_thread()
	{
		VERIFY_CLI();

		load_temp_stack();
		PageTable::kernel().load();

		Thread* current = &current_thread();

		while (current->state() == Thread::State::Terminated)
		{
			Thread* thread = m_current_thread->thread;
			if (thread->has_process())
				thread->process().on_thread_exit(*thread);
			
			remove_and_advance_current_thread();

			delete thread;

			current = &current_thread();
		}

		if (current->has_process())
		{
			current->process().page_table().load();
			GDT::set_tss_stack(current->interrupt_stack_base() + current->interrupt_stack_size());
		}
		else
			PageTable::kernel().load();

		switch (current->state())
		{
			case Thread::State::NotStarted:
				current->set_started();
				start_thread(current->rsp(), current->rip());
			case Thread::State::Executing:
				while (current->has_signal_to_execute())
					current->handle_signal();
				// fall through
			case Thread::State::Terminating:
				continue_thread(current->rsp(), current->rip());
			case Thread::State::Terminated:
				ASSERT_NOT_REACHED();
		}

		ASSERT_NOT_REACHED();
	}

	void Scheduler::set_current_thread_sleeping(uint64_t wake_time)
	{
		VERIFY_STI();
		DISABLE_INTERRUPTS();

		ASSERT(m_current_thread);

		Thread* sleeping = m_current_thread->thread;

		if (save_current_thread())
		{
			ENABLE_INTERRUPTS();
			return;
		}
		remove_and_advance_current_thread();

		auto it = m_sleeping_threads.begin();
		for (; it != m_sleeping_threads.end(); it++)
			if (wake_time <= it->wake_time)
				break;

		// This should work as we released enough memory from active thread
		static_assert(sizeof(ActiveThread) == sizeof(SleepingThread));
		MUST(m_sleeping_threads.emplace(it, sleeping, wake_time));

		execute_current_thread();
		ASSERT_NOT_REACHED();
	}

	void Scheduler::block_current_thread(Semaphore* semaphore)
	{
		VERIFY_STI();
		DISABLE_INTERRUPTS();

		ASSERT(m_current_thread);

		Thread* blocking = m_current_thread->thread;

		if (save_current_thread())
		{
			ENABLE_INTERRUPTS();
			return;
		}
		remove_and_advance_current_thread();

		// This should work as we released enough memory from active thread
		static_assert(sizeof(ActiveThread) == sizeof(BlockingThread));
		MUST(m_blocking_threads.emplace_back(blocking, semaphore));

		execute_current_thread();
		ASSERT_NOT_REACHED();
	}
	
	void Scheduler::unblock_threads(Semaphore* semaphore)
	{
		CriticalScope critical;

		for (auto it = m_blocking_threads.begin(); it != m_blocking_threads.end();)
		{
			if (it->semaphore == semaphore)
			{
				auto thread = it->thread;
				it = m_blocking_threads.remove(it);

				// This should work as we released enough memory from active thread
				static_assert(sizeof(ActiveThread) == sizeof(BlockingThread));
				MUST(m_active_threads.emplace_back(thread));
			}
			else
			{
				it++;
			}
		}
	}

	void Scheduler::unblock_thread(pid_t tid)
	{
		CriticalScope _;

		for (auto it = m_blocking_threads.begin(); it != m_blocking_threads.end(); it++)
		{
			if (it->thread->tid() == tid)
			{
				auto thread = it->thread;
				it = m_blocking_threads.remove(it);

				// This should work as we released enough memory from active thread
				static_assert(sizeof(ActiveThread) == sizeof(BlockingThread));
				MUST(m_active_threads.emplace_back(thread));

				return;
			}
		}
	}

	bool Scheduler::is_valid_tid(pid_t tid)
	{
		CriticalScope _;

		if (s_instance == nullptr)
			return tid == 0;

		for (auto& thread : s_instance->m_active_threads)
			if (thread.thread->tid() == tid)
				return true;
		
		for (auto& thread : s_instance->m_sleeping_threads)
			if (thread.thread->tid() == tid)
				return true;

		for (auto& thread : s_instance->m_blocking_threads)
			if (thread.thread->tid() == tid)
				return true;

		return false;
	}

}