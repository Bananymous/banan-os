#include <kernel/Arch.h>
#include <kernel/Attributes.h>
#include <kernel/GDT.h>
#include <kernel/InterruptController.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Timer/Timer.h>

#define SCHEDULER_VERIFY_STACK 1
#define SCHEDULER_VERIFY_INTERRUPT_STATE 1

#if SCHEDULER_VERIFY_INTERRUPT_STATE
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

	void SchedulerLock::lock()
	{
		auto tid = Scheduler::current_tid();
		if (tid != m_locker)
		{
			while (!m_locker.compare_exchange(-1, tid))
				__builtin_ia32_pause();
			ASSERT_EQ(m_lock_depth, 0);
		}
		m_lock_depth++;
	}

	void SchedulerLock::unlock()
	{
		ASSERT_EQ(m_locker.load(), Scheduler::current_tid());
		ASSERT_GT(m_lock_depth, 0);
		if (--m_lock_depth == 0)
			m_locker = -1;
	}

	void SchedulerLock::unlock_all()
	{
		ASSERT_EQ(m_locker.load(), Scheduler::current_tid());
		ASSERT_GT(m_lock_depth, 0);
		m_lock_depth = 0;
		m_locker = -1;
	}

	pid_t SchedulerLock::locker() const
	{
		return m_locker;
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
		m_lock.lock();
		execute_current_thread();
		ASSERT_NOT_REACHED();
	}

	Thread& Scheduler::current_thread()
	{
		return m_current_thread ? *m_current_thread->thread : *m_idle_thread;
	}

	pid_t Scheduler::current_tid()
	{
		if (s_instance == nullptr || s_instance->m_idle_thread == nullptr)
			return 0;
		return Scheduler::get().current_thread().tid();
	}

	void Scheduler::timer_reschedule()
	{
		VERIFY_CLI();
		m_lock.lock();

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
		m_lock.lock();

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
		m_lock.lock();

		if (m_active_threads.empty() || &current_thread() != m_idle_thread)
			return m_lock.unlock();

		if (save_current_thread())
			return;
		m_current_thread = {};
		advance_current_thread();
		execute_current_thread();
		ASSERT_NOT_REACHED();
	}

	void Scheduler::reschedule_current_no_save()
	{
		VERIFY_CLI();
		m_lock.lock();
		execute_current_thread();
	}

	void Scheduler::wake_threads()
	{
		VERIFY_CLI();
		ASSERT_EQ(m_lock.locker(), current_tid());

		uint64_t current_time = SystemTimer::get().ms_since_boot();
		while (!m_sleeping_threads.empty() && m_sleeping_threads.front().wake_time <= current_time)
		{
			m_sleeping_threads.move_element_to_other_linked_list(
				m_active_threads,
				m_active_threads.end(),
				m_sleeping_threads.begin()
			);
		}
	}

	BAN::ErrorOr<void> Scheduler::add_thread(Thread* thread)
	{
		LockGuard _(m_lock);
		TRY(m_active_threads.emplace_back(thread));
		return {};
	}

	void Scheduler::advance_current_thread()
	{
		VERIFY_CLI();
		ASSERT_EQ(m_lock.locker(), current_tid());

		if (m_active_threads.empty())
			m_current_thread = {};
		else if (!m_current_thread || ++m_current_thread == m_active_threads.end())
			m_current_thread = m_active_threads.begin();

		m_lock.m_locker = current_tid();
	}

	void Scheduler::remove_and_advance_current_thread()
	{
		VERIFY_CLI();
		ASSERT_EQ(m_lock.locker(), current_tid());

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

		m_lock.m_locker = current_tid();
	}

	// NOTE: this is declared always inline, so we don't corrupt the stack
	//       after getting the rsp
	ALWAYS_INLINE bool Scheduler::save_current_thread()
	{
		VERIFY_CLI();
		ASSERT_EQ(m_lock.locker(), current_tid());

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
		m_lock.lock();

		load_temp_stack();
		PageTable::kernel().load();

		Thread* thread = m_current_thread->thread;

		ASSERT(thread->has_process());
		delete &thread->process();

		remove_and_advance_current_thread();

		delete thread;

		execute_current_thread();
		ASSERT_NOT_REACHED();
	}

	void Scheduler::execute_current_thread()
	{
		VERIFY_CLI();
		ASSERT_EQ(m_lock.locker(), current_tid());

		load_temp_stack();
		PageTable::kernel().load();
		_execute_current_thread();
		ASSERT_NOT_REACHED();
	}

	NEVER_INLINE void Scheduler::_execute_current_thread()
	{
		VERIFY_CLI();
		ASSERT_EQ(m_lock.locker(), current_tid());

#if SCHEDULER_VERIFY_STACK
		vaddr_t rsp;
		read_rsp(rsp);
		ASSERT((vaddr_t)s_temp_stack <= rsp && rsp <= (vaddr_t)s_temp_stack + sizeof(s_temp_stack));
		ASSERT(&PageTable::current() == &PageTable::kernel());
#endif

		Thread* current = &current_thread();

#if __enable_sse
		if (current != Thread::sse_thread())
		{
			asm volatile(
				"movq %cr0, %rax;"
				"orq $(1 << 3), %rax;"
				"movq %rax, %cr0"
			);
		}
#endif

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
				m_lock.unlock_all();
				start_thread(current->rsp(), current->rip());
			case Thread::State::Executing:
				while (current->can_add_signal_to_execute())
					current->handle_signal();
				m_lock.unlock_all();
				continue_thread(current->rsp(), current->rip());
			case Thread::State::Terminated:
				ASSERT_NOT_REACHED();
		}

		ASSERT_NOT_REACHED();
	}

	void Scheduler::set_current_thread_sleeping_impl(uint64_t wake_time)
	{
		VERIFY_CLI();
		ASSERT_EQ(m_lock.locker(), current_tid());

		if (save_current_thread())
		{
			ENABLE_INTERRUPTS();
			return;
		}

		auto it = m_sleeping_threads.begin();
		for (; it != m_sleeping_threads.end(); it++)
			if (wake_time <= it->wake_time)
				break;

		m_current_thread->wake_time = wake_time;
		m_active_threads.move_element_to_other_linked_list(
			m_sleeping_threads,
			it,
			m_current_thread
		);

		m_current_thread = {};
		m_lock.m_locker = current_tid();
		advance_current_thread();

		execute_current_thread();
		ASSERT_NOT_REACHED();
	}

	void Scheduler::set_current_thread_sleeping(uint64_t wake_time)
	{
		VERIFY_STI();
		DISABLE_INTERRUPTS();
		m_lock.lock();

		ASSERT(m_current_thread);

		m_current_thread->semaphore = nullptr;
		set_current_thread_sleeping_impl(wake_time);
	}

	void Scheduler::block_current_thread(Semaphore* semaphore, uint64_t wake_time)
	{
		VERIFY_STI();
		DISABLE_INTERRUPTS();
		m_lock.lock();

		ASSERT(m_current_thread);

		m_current_thread->semaphore = semaphore;
		set_current_thread_sleeping_impl(wake_time);
	}

	void Scheduler::unblock_threads(Semaphore* semaphore)
	{
		LockGuard _(m_lock);

		for (auto it = m_sleeping_threads.begin(); it != m_sleeping_threads.end();)
		{
			if (it->semaphore == semaphore)
			{
				it = m_sleeping_threads.move_element_to_other_linked_list(
					m_active_threads,
					m_active_threads.end(),
					it
				);
			}
			else
			{
				it++;
			}
		}
	}

	void Scheduler::unblock_thread(pid_t tid)
	{
		LockGuard _(m_lock);

		for (auto it = m_sleeping_threads.begin(); it != m_sleeping_threads.end(); it++)
		{
			if (it->thread->tid() == tid)
			{
				m_sleeping_threads.move_element_to_other_linked_list(
					m_active_threads,
					m_active_threads.end(),
					it
				);
				return;
			}
		}
	}

}
