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

	extern "C" [[noreturn]] void start_kernel_thread();
	extern "C" [[noreturn]] void start_userspace_thread();

	extern "C" void signal_trampoline();

	template<typename T>
	static void write_to_stack(uintptr_t& rsp, const T& value) requires(sizeof(T) <= sizeof(uintptr_t))
	{
		rsp -= sizeof(uintptr_t);
		*(uintptr_t*)rsp = (uintptr_t)value;
	}

	extern "C" uintptr_t get_thread_start_sp()
	{
		return Thread::current().interrupt_stack().sp;
	}

	extern "C" uintptr_t get_userspace_thread_stack_top()
	{
		return Thread::current().userspace_stack_top() - 4 * sizeof(uintptr_t);
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
		thread->m_kernel_stack = TRY(VirtualRange::create_to_vaddr_range(
			PageTable::kernel(),
			KERNEL_OFFSET,
			~(uintptr_t)0,
			m_kernel_stack_size,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			true
		));

		// Initialize stack for returning
		uintptr_t sp = thread->kernel_stack_top();
		write_to_stack(sp, thread);
		write_to_stack(sp, &Thread::on_exit_trampoline);
		write_to_stack(sp, data);
		write_to_stack(sp, entry);

		thread->m_interrupt_stack.ip = reinterpret_cast<vaddr_t>(start_kernel_thread);
		thread->m_interrupt_stack.cs = 0x08;
		thread->m_interrupt_stack.flags = 0x002;
		thread->m_interrupt_stack.sp = sp;
		thread->m_interrupt_stack.ss = 0x10;

		memset(&thread->m_interrupt_registers, 0, sizeof(InterruptRegisters));

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

		thread->m_kernel_stack = TRY(VirtualRange::create_to_vaddr_range(
			process->page_table(),
			0x300000, KERNEL_OFFSET,
			m_kernel_stack_size,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			true
		));

		thread->m_userspace_stack = TRY(VirtualRange::create_to_vaddr_range(
			process->page_table(),
			0x300000, KERNEL_OFFSET,
			m_userspace_stack_size,
			PageTable::Flags::UserSupervisor | PageTable::Flags::ReadWrite | PageTable::Flags::Present,
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
	#elif ARCH(i686)
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

	const Process& Thread::process() const
	{
		ASSERT(m_process);
		return *m_process;
	}

	Thread::~Thread()
	{
		if (m_delete_process)
		{
			ASSERT(m_process);
			delete m_process;
		}
	}

	BAN::ErrorOr<Thread*> Thread::clone(Process* new_process, uintptr_t sp, uintptr_t ip)
	{
		ASSERT(m_is_userspace);
		ASSERT(m_state == State::Executing);

		Thread* thread = new Thread(s_next_tid++, new_process);
		if (thread == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		BAN::ScopeGuard thread_deleter([thread] { delete thread; });

		thread->m_is_userspace = true;

		thread->m_kernel_stack = TRY(m_kernel_stack->clone(new_process->page_table()));
		thread->m_userspace_stack = TRY(m_userspace_stack->clone(new_process->page_table()));

		thread->m_state = State::NotStarted;

		thread->m_interrupt_stack.ip = ip;
		thread->m_interrupt_stack.cs = 0x08;
		thread->m_interrupt_stack.flags = 0x002;
		thread->m_interrupt_stack.sp = sp;
		thread->m_interrupt_stack.ss = 0x10;

#if ARCH(x86_64)
		thread->m_interrupt_registers.rax = 0;
#elif ARCH(i686)
		thread->m_interrupt_registers.eax = 0;
#endif

		thread_deleter.disable();

		return thread;
	}

	void Thread::setup_exec()
	{
		ASSERT(is_userspace());
		m_state = State::NotStarted;

		// Signal mask is inherited

		auto& userspace_info = process().userspace_info();
		ASSERT(userspace_info.entry);

		// Initialize stack for returning
		PageTable::with_fast_page(process().page_table().physical_address_of(kernel_stack_top() - PAGE_SIZE), [&] {
			uintptr_t sp = PageTable::fast_page() + PAGE_SIZE;
			write_to_stack(sp, userspace_info.entry);
			write_to_stack(sp, userspace_info.argc);
			write_to_stack(sp, userspace_info.argv);
			write_to_stack(sp, userspace_info.envp);
		});

		m_interrupt_stack.ip = reinterpret_cast<vaddr_t>(start_userspace_thread);;
		m_interrupt_stack.cs = 0x08;
		m_interrupt_stack.flags = 0x002;
		m_interrupt_stack.sp = kernel_stack_top() - 4 * sizeof(uintptr_t);
		m_interrupt_stack.ss = 0x10;

		memset(&m_interrupt_registers, 0, sizeof(InterruptRegisters));
	}

	void Thread::setup_process_cleanup()
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);

		m_state = State::NotStarted;
		static entry_t entry(
			[](void* process_ptr)
			{
				auto* thread = &Thread::current();
				auto* process = static_cast<Process*>(process_ptr);

				ASSERT(thread->m_process == process);

				process->cleanup_function();

				thread->m_delete_process = true;

				// will call on thread exit after return
			}
		);

		m_signal_pending_mask = 0;
		m_signal_block_mask = ~0ull;

		PageTable::with_fast_page(process().page_table().physical_address_of(kernel_stack_top() - PAGE_SIZE), [&] {
			uintptr_t sp = PageTable::fast_page() + PAGE_SIZE;
			write_to_stack(sp, this);
			write_to_stack(sp, &Thread::on_exit_trampoline);
			write_to_stack(sp, m_process);
			write_to_stack(sp, entry);
		});

		m_interrupt_stack.ip = reinterpret_cast<vaddr_t>(start_kernel_thread);
		m_interrupt_stack.cs = 0x08;
		m_interrupt_stack.flags = 0x002;
		m_interrupt_stack.sp = kernel_stack_top() - 4 * sizeof(uintptr_t);
		m_interrupt_stack.ss = 0x10;

		memset(&m_interrupt_registers, 0, sizeof(InterruptRegisters));
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
		auto& interrupt_stack = *reinterpret_cast<InterruptStack*>(kernel_stack_top() - sizeof(InterruptStack));
		if (!GDT::is_user_segment(interrupt_stack.cs))
			return false;
		uint64_t full_pending_mask = m_signal_pending_mask | process().signal_pending_mask();;
		return full_pending_mask & ~m_signal_block_mask;
	}

	bool Thread::will_execute_signal() const
	{
		if (!is_userspace() || m_state != State::Executing)
			return false;
		auto& interrupt_stack = *reinterpret_cast<InterruptStack*>(kernel_stack_top() - sizeof(InterruptStack));
		return interrupt_stack.ip == (uintptr_t)signal_trampoline;
	}

	void Thread::handle_signal(int signal)
	{
		ASSERT(&Thread::current() == this);
		ASSERT(is_userspace());

		SpinLockGuard _(m_signal_lock);

		auto& interrupt_stack = *reinterpret_cast<InterruptStack*>(kernel_stack_top() - sizeof(InterruptStack));
		ASSERT(GDT::is_user_segment(interrupt_stack.cs));

		if (signal == 0)
		{
			uint64_t full_pending_mask = m_signal_pending_mask | process().signal_pending_mask();
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
		process().remove_pending_signal(signal);

		if (signal_handler == (vaddr_t)SIG_IGN)
			;
		else if (signal_handler != (vaddr_t)SIG_DFL)
		{
			// call userspace signal handlers
			interrupt_stack.sp -= 128; // skip possible red-zone
			write_to_stack(interrupt_stack.sp, interrupt_stack.ip);
			write_to_stack(interrupt_stack.sp, signal);
			write_to_stack(interrupt_stack.sp, signal_handler);
			interrupt_stack.ip = (uintptr_t)signal_trampoline;
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

	void Thread::on_exit_trampoline(Thread* thread)
	{
		thread->on_exit();
	}

	void Thread::on_exit()
	{
		ASSERT(this == &Thread::current());
		if (!m_delete_process && has_process())
		{
			if (process().on_thread_exit(*this))
			{
				Processor::set_interrupt_state(InterruptState::Disabled);
				setup_process_cleanup();
				Scheduler::get().yield();
			}
			else
				Scheduler::get().terminate_thread(this);
		}
		else
		{
			Scheduler::get().terminate_thread(this);
		}
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
