#include <kernel/Arch.h>
#include <kernel/Attributes.h>
#include <kernel/GDT.h>
#include <kernel/InterruptController.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Timer/Timer.h>

#define SCHEDULER_VERIFY_STACK 1

namespace Kernel
{

	extern "C" [[noreturn]] void start_thread(uintptr_t sp, uintptr_t ip);
	extern "C" [[noreturn]] void continue_thread(uintptr_t sp, uintptr_t ip);

	static Scheduler* s_instance = nullptr;
	static BAN::Atomic<bool> s_started { false };

	ALWAYS_INLINE static void load_temp_stack()
	{
#if ARCH(x86_64)
		asm volatile("movq %0, %%rsp" :: "rm"(Processor::current_stack_top()));
#elif ARCH(i686)
		asm volatile("movl %0, %%esp" :: "rm"(Processor::current_stack_top()));
#else
		#error
#endif
	}

	BAN::ErrorOr<void> Scheduler::initialize()
	{
		ASSERT(s_instance == nullptr);
		s_instance = new Scheduler();
		ASSERT(s_instance);
		Processor::allocate_idle_thread();
		return {};
	}

	Scheduler& Scheduler::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	void Scheduler::start()
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);
		m_lock.lock();
		s_started = true;
		advance_current_thread();
		execute_current_thread_locked();
		ASSERT_NOT_REACHED();
	}

	bool Scheduler::is_started()
	{
		return s_started;
	}

	Thread& Scheduler::current_thread()
	{
		auto* current = Processor::get_current_thread();
		return current ? *current->thread : *Processor::idle_thread();
	}

	pid_t Scheduler::current_tid()
	{
		if (s_instance == nullptr)
			return 0;
		return Scheduler::get().current_thread().tid();
	}

	void Scheduler::timer_reschedule()
	{
		// Broadcast IPI to all other processors for them
		// to perform reschedule
		InterruptController::get().broadcast_ipi();

		auto state = m_lock.lock();
		m_blocking_threads.remove_with_wake_time(m_active_threads, SystemTimer::get().ms_since_boot());
		if (save_current_thread())
			return Processor::set_interrupt_state(state);
		advance_current_thread();
		execute_current_thread_locked();
		ASSERT_NOT_REACHED();
	}

	void Scheduler::reschedule()
	{
		auto state = m_lock.lock();
		if (save_current_thread())
			return Processor::set_interrupt_state(state);
		advance_current_thread();
		execute_current_thread_locked();
		ASSERT_NOT_REACHED();
	}

	void Scheduler::reschedule_if_idling()
	{
		auto state = m_lock.lock();
		if (m_active_threads.empty() || Processor::get_current_thread())
			return m_lock.unlock(state);
		if (save_current_thread())
			return Processor::set_interrupt_state(state);
		advance_current_thread();
		execute_current_thread_locked();
		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<void> Scheduler::add_thread(Thread* thread)
	{
		auto* node = new SchedulerQueue::Node(thread);
		if (node == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		SpinLockGuard _(m_lock);
		m_active_threads.push_back(node);
		return {};
	}

	void Scheduler::terminate_thread(Thread* thread)
	{
		SpinLockGuard _(m_lock);
		thread->m_state = Thread::State::Terminated;
		if (thread == &current_thread())
			execute_current_thread_locked();
	}

	void Scheduler::advance_current_thread()
	{
		ASSERT(m_lock.current_processor_has_lock());

		if (auto* current = Processor::get_current_thread())
			m_active_threads.push_back(current);
		Processor::set_current_thread(nullptr);

		if (!m_active_threads.empty())
			Processor::set_current_thread(m_active_threads.pop_front());
	}

	// NOTE: this is declared always inline, so we don't corrupt the stack
	//       after getting the rsp
	ALWAYS_INLINE bool Scheduler::save_current_thread()
	{
		ASSERT(m_lock.current_processor_has_lock());

		uintptr_t sp, ip;
		push_callee_saved();
		if (!(ip = read_ip()))
		{
			pop_callee_saved();
			return true;
		}
		read_rsp(sp);

		Thread& current = current_thread();
		current.set_ip(ip);
		current.set_sp(sp);

		load_temp_stack();

		return false;
	}

	void Scheduler::delete_current_process_and_thread()
	{
		m_lock.lock();

		load_temp_stack();
		PageTable::kernel().load();

		auto* current = Processor::get_current_thread();
		ASSERT(current);
		delete &current->thread->process();
		delete current->thread;
		delete current;
		Processor::set_current_thread(nullptr);

		advance_current_thread();
		execute_current_thread_locked();
		ASSERT_NOT_REACHED();
	}

	void Scheduler::execute_current_thread()
	{
		m_lock.lock();
		load_temp_stack();
		PageTable::kernel().load();
		execute_current_thread_stack_loaded();
		ASSERT_NOT_REACHED();
	}

	void Scheduler::execute_current_thread_locked()
	{
		ASSERT(m_lock.current_processor_has_lock());
		load_temp_stack();
		PageTable::kernel().load();
		execute_current_thread_stack_loaded();
		ASSERT_NOT_REACHED();
	}

	NEVER_INLINE void Scheduler::execute_current_thread_stack_loaded()
	{
		ASSERT(m_lock.current_processor_has_lock());

#if SCHEDULER_VERIFY_STACK
		vaddr_t rsp;
		read_rsp(rsp);
		ASSERT(Processor::current_stack_bottom() <= rsp && rsp <= Processor::current_stack_top());
		ASSERT(&PageTable::current() == &PageTable::kernel());
#endif

		Thread* current = &current_thread();

#if __enable_sse
		if (current != Thread::sse_thread())
		{
#if ARCH(x86_64)
			asm volatile(
				"movq %cr0, %rax;"
				"orq $(1 << 3), %rax;"
				"movq %rax, %cr0"
			);
#elif ARCH(i686)
			asm volatile(
				"movl %cr0, %eax;"
				"orl $(1 << 3), %eax;"
				"movl %eax, %cr0"
			);
#else
			#error
#endif
		}
#endif

		while (current->state() == Thread::State::Terminated)
		{
			auto* node = Processor::get_current_thread();
			if (node->thread->has_process())
				if (node->thread->process().on_thread_exit(*node->thread))
					break;

			delete node->thread;
			delete node;
			Processor::set_current_thread(nullptr);

			advance_current_thread();
			current = &current_thread();
		}

		if (current->has_process())
		{
			current->process().page_table().load();
			Processor::gdt().set_tss_stack(current->interrupt_stack_base() + current->interrupt_stack_size());
		}
		else
			PageTable::kernel().load();

		switch (current->state())
		{
			case Thread::State::NotStarted:
				current->set_started();
				m_lock.unlock(InterruptState::Disabled);
				start_thread(current->sp(), current->ip());
			case Thread::State::Executing:
				m_lock.unlock(InterruptState::Disabled);
				while (current->can_add_signal_to_execute())
					current->handle_signal();
				continue_thread(current->sp(), current->ip());
			case Thread::State::Terminated:
				ASSERT_NOT_REACHED();
		}

		ASSERT_NOT_REACHED();
	}

	void Scheduler::set_current_thread_sleeping_impl(Semaphore* semaphore, uint64_t wake_time)
	{
		ASSERT(m_lock.current_processor_has_lock());

		if (save_current_thread())
			return;

		auto* current = Processor::get_current_thread();
		current->semaphore = semaphore;
		current->wake_time = wake_time;
		m_blocking_threads.add_with_wake_time(current);
		Processor::set_current_thread(nullptr);

		advance_current_thread();
		execute_current_thread_locked();
		ASSERT_NOT_REACHED();
	}

	void Scheduler::set_current_thread_sleeping(uint64_t wake_time)
	{
		auto state = m_lock.lock();
		set_current_thread_sleeping_impl(nullptr, wake_time);
		Processor::set_interrupt_state(state);
	}

	void Scheduler::block_current_thread(Semaphore* semaphore, uint64_t wake_time)
	{
		auto state = m_lock.lock();
		set_current_thread_sleeping_impl(semaphore, wake_time);
		Processor::set_interrupt_state(state);
	}

	void Scheduler::unblock_threads(Semaphore* semaphore)
	{
		SpinLockGuard _(m_lock);
		m_blocking_threads.remove_with_condition(m_active_threads, [&](auto* node) { return node->semaphore == semaphore; });
	}

	void Scheduler::unblock_thread(pid_t tid)
	{
		SpinLockGuard _(m_lock);
		m_blocking_threads.remove_with_condition(m_active_threads, [&](auto* node) { return node->thread->tid() == tid; });
	}

}
