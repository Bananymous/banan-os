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

	static Scheduler* s_instance = nullptr;
	static BAN::Atomic<bool> s_started { false };

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
		ASSERT(!m_active_threads.empty());
		yield();
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

	void Scheduler::setup_next_thread()
	{
		ASSERT(m_lock.current_processor_has_lock());

		if (auto* current = Processor::get_current_thread())
		{
			auto* thread = current->thread;

			if (thread->state() == Thread::State::Terminated)
			{
				PageTable::kernel().load();
				delete thread;
				delete current;
			}
			else
			{
				// thread->state() can be NotStarted when calling exec or cleaning up process
				if (thread->state() != Thread::State::NotStarted)
				{
					thread->interrupt_stack() = Processor::get_interrupt_stack();
					thread->interrupt_registers() = Processor::get_interrupt_registers();
				}

				if (current->should_block)
				{
					current->should_block = false;
					m_blocking_threads.add_with_wake_time(current);
				}
				else
				{
					m_active_threads.push_back(current);
				}
			}
		}

		SchedulerQueue::Node* node = nullptr;
		while (!m_active_threads.empty())
		{
			node = m_active_threads.pop_front();
			if (node->thread->state() != Thread::State::Terminated)
				break;

			PageTable::kernel().load();
			delete node->thread;
			delete node;
			node = nullptr;
		}

		Processor::set_current_thread(node);

		auto* thread = node ? node->thread : Processor::idle_thread();

		if (thread->has_process())
			thread->process().page_table().load();
		else
			PageTable::kernel().load();

		if (thread->state() == Thread::State::NotStarted)
			thread->m_state = Thread::State::Executing;

		ASSERT(thread->interrupt_stack().ip);
		ASSERT(thread->interrupt_stack().sp);

		Processor::gdt().set_tss_stack(thread->kernel_stack_top());

		Processor::get_interrupt_stack() = thread->interrupt_stack();
		Processor::get_interrupt_registers() = thread->interrupt_registers();
	}

	void Scheduler::timer_reschedule()
	{
		// Broadcast IPI to all other processors for them
		// to perform reschedule
		InterruptController::get().broadcast_ipi();

		{
			SpinLockGuard _(m_lock);
			m_blocking_threads.remove_with_wake_time(m_active_threads, SystemTimer::get().ms_since_boot());
		}

		yield();
	}

	void Scheduler::yield()
	{
		auto state = Processor::get_interrupt_state();
		Processor::set_interrupt_state(InterruptState::Disabled);

#if ARCH(x86_64)
		asm volatile(
			"movq %%rsp, %%rcx;"
			"movq %[load_sp], %%rsp;"
			"int %[ipi];"
			"movq %%rcx, %%rsp;"
			:: [load_sp]"r"(Processor::current_stack_top()),
			   [ipi]"i"(IRQ_VECTOR_BASE + IRQ_IPI)
			:  "memory", "rcx"
		);
#elif ARCH(i686)
		asm volatile(
			"movl %%esp, %%ecx;"
			"movl %[load_sp], %%esp;"
			"int %[ipi];"
			"movl %%ecx, %%esp;"
			:: [load_sp]"r"(Processor::current_stack_top()),
			   [ipi]"i"(IRQ_VECTOR_BASE + IRQ_IPI)
			:  "memory", "ecx"
		);
#else
		#error
#endif

		Processor::set_interrupt_state(state);
	}

	void Scheduler::irq_reschedule()
	{
		SpinLockGuard _(m_lock);
		setup_next_thread();
	}

	void Scheduler::reschedule_if_idling()
	{
		{
			SpinLockGuard _(m_lock);
			if (Processor::get_current_thread())
				return;
			if (m_active_threads.empty())
				return;
		}

		yield();
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
		auto state = m_lock.lock();

		ASSERT(thread->state() == Thread::State::Executing);
		thread->m_state = Thread::State::Terminated;
		thread->interrupt_stack().sp = Processor::current_stack_top();

		m_lock.unlock(InterruptState::Disabled);

		// actual deletion will be done while rescheduling

		if (&current_thread() == thread)
		{
			yield();
			ASSERT_NOT_REACHED();
		}

		Processor::set_interrupt_state(state);
	}

	void Scheduler::set_current_thread_sleeping_impl(Semaphore* semaphore, uint64_t wake_time)
	{
		auto state = m_lock.lock();

		auto* current = Processor::get_current_thread();
		current->semaphore = semaphore;
		current->wake_time = wake_time;
		current->should_block = true;

		m_lock.unlock(InterruptState::Disabled);

		yield();

		Processor::set_interrupt_state(state);
	}

	void Scheduler::set_current_thread_sleeping(uint64_t wake_time)
	{
		set_current_thread_sleeping_impl(nullptr, wake_time);
	}

	void Scheduler::block_current_thread(Semaphore* semaphore, uint64_t wake_time)
	{
		set_current_thread_sleeping_impl(semaphore, wake_time);
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
