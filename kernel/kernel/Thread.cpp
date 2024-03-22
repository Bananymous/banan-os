#include <BAN/Errors.h>
#include <BAN/ScopeGuard.h>
#include <kernel/GDT.h>
#include <kernel/InterruptController.h>
#include <kernel/InterruptStack.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Thread.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	extern "C" void thread_userspace_trampoline(uint64_t rsp, uint64_t rip, int argc, char** argv, char** envp);
	extern "C" uintptr_t read_rip();

	extern "C" void signal_trampoline();

	template<typename T>
	static void write_to_stack(uintptr_t& rsp, const T& value)
	{
		rsp -= sizeof(uintptr_t);
		if constexpr(sizeof(T) < sizeof(uintptr_t))
			*(uintptr_t*)rsp = (uintptr_t)value;
		else
			memcpy((void*)rsp, (void*)&value, sizeof(uintptr_t));
	}

	static pid_t s_next_tid = 1;

	BAN::ErrorOr<Thread*> Thread::create_kernel(entry_t entry, void* data, Process* process)
	{
		// Create the thread object
		Thread* thread = new Thread(s_next_tid++, process);
		if (thread == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		BAN::ScopeGuard thread_deleter([thread] { delete thread; });

		// Initialize stack and registers
		thread->m_stack = TRY(VirtualRange::create_to_vaddr_range(
			PageTable::kernel(),
			KERNEL_OFFSET,
			~(uintptr_t)0,
			m_kernel_stack_size,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			true
		));
		thread->m_rsp = thread->stack_base() + thread->stack_size();
		thread->m_rip = (uintptr_t)entry;

		// Initialize stack for returning
		write_to_stack(thread->m_rsp, nullptr); // alignment
		write_to_stack(thread->m_rsp, thread);
		write_to_stack(thread->m_rsp, &Thread::on_exit);
		write_to_stack(thread->m_rsp, data);

		thread_deleter.disable();

		return thread;
	}

	BAN::ErrorOr<Thread*> Thread::create_userspace(Process* process)
	{
		ASSERT(process);

		// Create the thread object
		Thread* thread = new Thread(s_next_tid++, process);
		if (thread == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		BAN::ScopeGuard thread_deleter([thread] { delete thread; });

		thread->m_is_userspace = true;

		thread->m_stack = TRY(VirtualRange::create_to_vaddr_range(
			process->page_table(),
			0x300000, KERNEL_OFFSET,
			m_userspace_stack_size,
			PageTable::Flags::UserSupervisor | PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			true
		));

		thread->m_interrupt_stack = TRY(VirtualRange::create_to_vaddr_range(
			process->page_table(),
			0x300000, KERNEL_OFFSET,
			m_interrupt_stack_size,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			true
		));

		thread->setup_exec();

		thread_deleter.disable();

		return thread;
	}

	Thread::Thread(pid_t tid, Process* process)
		: m_tid(tid), m_process(process)
	{
#if __enable_sse
	#if ARCH(x86_64)
		uintptr_t cr0;
		asm volatile(
			"movq %%cr0, %%rax;"
			"movq %%rax, %[cr0];"
			"andq $~(1 << 3), %%rax;"
			"movq %%rax, %%cr0;"
			: [cr0]"=r"(cr0)
			:: "rax"
		);
		save_sse();
		asm volatile("movq %0, %%cr0" :: "r"(cr0));
	#elif ARCH(i386)
		uintptr_t cr0;
		asm volatile(
			"movl %%cr0, %%eax;"
			"movl %%eax, %[cr0];"
			"andl $~(1 << 3), %%eax;"
			"movl %%eax, %%cr0;"
			: [cr0]"=r"(cr0)
			:: "eax"
		);
		save_sse();
		asm volatile("movl %0, %%cr0" :: "r"(cr0));
	#else
		#error
	#endif
#endif
	}

	Thread& Thread::current()
	{
		return Scheduler::get().current_thread();
	}

	Process& Thread::process()
	{
		ASSERT(m_process);
		return *m_process;
	}

	Thread::~Thread()
	{
	}

	BAN::ErrorOr<Thread*> Thread::clone(Process* new_process, uintptr_t rsp, uintptr_t rip)
	{
		ASSERT(m_is_userspace);
		ASSERT(m_state == State::Executing);

		Thread* thread = new Thread(s_next_tid++, new_process);
		if (thread == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		BAN::ScopeGuard thread_deleter([thread] { delete thread; });

		thread->m_is_userspace = true;

		thread->m_interrupt_stack = TRY(m_interrupt_stack->clone(new_process->page_table()));
		thread->m_stack = TRY(m_stack->clone(new_process->page_table()));

		thread->m_state = State::Executing;

		thread->m_rip = rip;
		thread->m_rsp = rsp;

		thread_deleter.disable();

		return thread;
	}

	void Thread::setup_exec()
	{
		ASSERT(is_userspace());
		m_state = State::NotStarted;
		static entry_t entry_trampoline(
			[](void*)
			{
				const auto& info = Process::current().userspace_info();
				thread_userspace_trampoline(Thread::current().rsp(), info.entry, info.argc, info.argv, info.envp);
				ASSERT_NOT_REACHED();
			}
		);
		m_rsp = stack_base() + stack_size();
		m_rip = (uintptr_t)entry_trampoline;

		// Signal mask is inherited

		// Setup stack for returning
		ASSERT(m_rsp % PAGE_SIZE == 0);
		PageTable::with_fast_page(process().page_table().physical_address_of(m_rsp - PAGE_SIZE), [&] {
			uintptr_t rsp = PageTable::fast_page() + PAGE_SIZE;
			write_to_stack(rsp, nullptr); // alignment
			write_to_stack(rsp, this);
			write_to_stack(rsp, &Thread::on_exit);
			write_to_stack(rsp, nullptr);
			m_rsp -= 4 * sizeof(uintptr_t);
		});
	}

	void Thread::setup_process_cleanup()
	{
		m_state = State::NotStarted;
		static entry_t entry(
			[](void* process_ptr)
			{
				auto& process = *reinterpret_cast<Process*>(process_ptr);
				process.cleanup_function();
				Scheduler::get().delete_current_process_and_thread();
				ASSERT_NOT_REACHED();
			}
		);
		m_rsp = stack_base() + stack_size();
		m_rip = (uintptr_t)entry;

		m_signal_pending_mask = 0;
		m_signal_block_mask = ~0ull;

		ASSERT(m_rsp % PAGE_SIZE == 0);
		PageTable::with_fast_page(process().page_table().physical_address_of(m_rsp - PAGE_SIZE), [&] {
			uintptr_t rsp = PageTable::fast_page() + PAGE_SIZE;
			write_to_stack(rsp, nullptr); // alignment
			write_to_stack(rsp, this);
			write_to_stack(rsp, &Thread::on_exit);
			write_to_stack(rsp, m_process);
			m_rsp -= 4 * sizeof(uintptr_t);
		});
	}

	bool Thread::is_interrupted_by_signal()
	{
		while (can_add_signal_to_execute())
			handle_signal();
		return will_execute_signal();
	}

	bool Thread::can_add_signal_to_execute() const
	{
		if (!is_userspace() || m_state != State::Executing)
			return false;
		auto& interrupt_stack = *reinterpret_cast<InterruptStack*>(interrupt_stack_base() + interrupt_stack_size() - sizeof(InterruptStack));
		if (!GDT::is_user_segment(interrupt_stack.cs))
			return false;
		uint64_t full_pending_mask = m_signal_pending_mask | m_process->m_signal_pending_mask;
		return full_pending_mask & ~m_signal_block_mask;
	}

	bool Thread::will_execute_signal() const
	{
		if (!is_userspace() || m_state != State::Executing)
			return false;
		auto& interrupt_stack = *reinterpret_cast<InterruptStack*>(interrupt_stack_base() + interrupt_stack_size() - sizeof(InterruptStack));
		return interrupt_stack.rip == (uintptr_t)signal_trampoline;
	}

	void Thread::handle_signal(int signal)
	{
		ASSERT(&Thread::current() == this);
		ASSERT(is_userspace());

		SpinLockGuard _(m_signal_lock);

		auto& interrupt_stack = *reinterpret_cast<InterruptStack*>(interrupt_stack_base() + interrupt_stack_size() - sizeof(InterruptStack));
		ASSERT(GDT::is_user_segment(interrupt_stack.cs));

		if (signal == 0)
		{
			uint64_t full_pending_mask = m_signal_pending_mask | process().m_signal_pending_mask;
			for (signal = _SIGMIN; signal <= _SIGMAX; signal++)
			{
				uint64_t mask = 1ull << signal;
				if ((full_pending_mask & mask) && !(m_signal_block_mask & mask))
					break;
			}
			ASSERT(signal <= _SIGMAX);
		}
		else
		{
			ASSERT(signal >= _SIGMIN);
			ASSERT(signal <= _SIGMAX);
		}

		vaddr_t signal_handler = process().m_signal_handlers[signal];

		m_signal_pending_mask &= ~(1ull << signal);
		process().m_signal_pending_mask &= ~(1ull << signal);

		if (signal_handler == (vaddr_t)SIG_IGN)
			;
		else if (signal_handler != (vaddr_t)SIG_DFL)
		{
			// call userspace signal handlers
			interrupt_stack.rsp -= 128; // skip possible red-zone
			write_to_stack(interrupt_stack.rsp, interrupt_stack.rip);
			write_to_stack(interrupt_stack.rsp, signal);
			write_to_stack(interrupt_stack.rsp, signal_handler);
			interrupt_stack.rip = (uintptr_t)signal_trampoline;
		}
		else
		{
			switch (signal)
			{
				// Abnormal termination of the process with additional actions.
				case SIGABRT:
				case SIGBUS:
				case SIGFPE:
				case SIGILL:
				case SIGQUIT:
				case SIGSEGV:
				case SIGSYS:
				case SIGTRAP:
				case SIGXCPU:
				case SIGXFSZ:
					process().exit(128 + signal, signal | 0x80);
					break;

				// Abnormal termination of the process
				case SIGALRM:
				case SIGHUP:
				case SIGINT:
				case SIGKILL:
				case SIGPIPE:
				case SIGTERM:
				case SIGUSR1:
				case SIGUSR2:
				case SIGPOLL:
				case SIGPROF:
				case SIGVTALRM:
					process().exit(128 + signal, signal);
					break;

				// Ignore the signal
				case SIGCHLD:
				case SIGURG:
					break;

				// Stop the process:
				case SIGTSTP:
				case SIGTTIN:
				case SIGTTOU:
					ASSERT_NOT_REACHED();

				// Continue the process, if it is stopped; otherwise, ignore the signal.
				case SIGCONT:
					ASSERT_NOT_REACHED();
			}
		}
	}

	bool Thread::add_signal(int signal)
	{
		SpinLockGuard _(m_signal_lock);

		uint64_t mask = 1ull << signal;
		if (!(m_signal_block_mask & mask))
		{
			m_signal_pending_mask |= mask;
			if (this != &Thread::current())
				Scheduler::get().unblock_thread(tid());
			return true;
		}
		return false;
	}

	BAN::ErrorOr<void> Thread::block_or_eintr_indefinite(Semaphore& semaphore)
	{
		if (is_interrupted_by_signal())
			return BAN::Error::from_errno(EINTR);
		semaphore.block_indefinite();
		if (is_interrupted_by_signal())
			return BAN::Error::from_errno(EINTR);
		return {};
	}

	BAN::ErrorOr<void> Thread::block_or_eintr_or_timeout(Semaphore& semaphore, uint64_t timeout_ms, bool etimedout)
	{
		uint64_t wake_time_ms = SystemTimer::get().ms_since_boot() + timeout_ms;
		return block_or_eintr_or_waketime(semaphore, wake_time_ms, etimedout);
	}

	BAN::ErrorOr<void> Thread::block_or_eintr_or_waketime(Semaphore& semaphore, uint64_t wake_time_ms, bool etimedout)
	{
		if (is_interrupted_by_signal())
			return BAN::Error::from_errno(EINTR);
		semaphore.block_with_wake_time(wake_time_ms);
		if (is_interrupted_by_signal())
			return BAN::Error::from_errno(EINTR);
		if (etimedout && SystemTimer::get().ms_since_boot() >= wake_time_ms)
			return BAN::Error::from_errno(ETIMEDOUT);
		return {};
	}

	void Thread::validate_stack() const
	{
		if (stack_base() <= m_rsp && m_rsp <= stack_base() + stack_size())
			return;
		if (interrupt_stack_base() <= m_rsp && m_rsp <= interrupt_stack_base() + interrupt_stack_size())
			return;
		Kernel::panic("rsp {8H}, stack {8H}->{8H}, interrupt_stack {8H}->{8H}", m_rsp,
			stack_base(), stack_base() + stack_size(),
			interrupt_stack_base(), interrupt_stack_base() + interrupt_stack_size()
		);
	}

	void Thread::on_exit()
	{
		ASSERT(this == &Thread::current());
		Scheduler::get().terminate_thread(this);
		ASSERT_NOT_REACHED();
	}

#if __enable_sse
	static Thread* s_sse_thread = nullptr;

	void Thread::save_sse()
	{
		asm volatile("fxsave %0" :: "m"(m_sse_storage));
	}

	void Thread::load_sse()
	{
		asm volatile("fxrstor %0" :: "m"(m_sse_storage));
		s_sse_thread = this;
	}

	Thread* Thread::sse_thread()
	{
		return s_sse_thread;
	}
#endif

}
