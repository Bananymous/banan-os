#include <BAN/Errors.h>
#include <BAN/ScopeGuard.h>
#include <kernel/InterruptController.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Memory/PageTableScope.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Thread.h>

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


	Thread::TerminateBlocker::TerminateBlocker(Thread& thread)
		: m_thread(thread)
	{
		{
			CriticalScope _;

			if (m_thread.state() == State::Executing || m_thread.m_terminate_blockers > 0)
			{
				m_thread.m_terminate_blockers++;
				return;
			}

			if (m_thread.state() == State::Terminating && m_thread.m_terminate_blockers == 0)
				m_thread.m_state = State::Terminated;
		}

		while (true)
			Scheduler::get().reschedule();
	}

	Thread::TerminateBlocker::~TerminateBlocker()
	{
		{
			CriticalScope _;
	
			m_thread.m_terminate_blockers--;

			if (m_thread.state() == State::Executing || m_thread.m_terminate_blockers > 0)
				return;

			if (m_thread.state() == State::Terminating && m_thread.m_terminate_blockers == 0)
				m_thread.m_state = State::Terminated;
		}

		while (true)
			Scheduler::get().reschedule();
	}

	void Thread::set_terminating()
	{
		CriticalScope _;
		m_state = m_terminate_blockers == 0 ? State::Terminated : State::Terminating;
		Scheduler::get().unblock_thread(tid());
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
		thread->m_stack = TRY(VirtualRange::create_kmalloc(m_kernel_stack_size));
		thread->m_rsp = thread->stack_base() + thread->stack_size();
		thread->m_rip = (uintptr_t)entry;

		// Initialize stack for returning
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

		thread->m_stack = TRY(VirtualRange::create(process->page_table(), 0, m_userspace_stack_size, PageTable::Flags::UserSupervisor | PageTable::Flags::ReadWrite | PageTable::Flags::Present));
		thread->m_interrupt_stack = TRY(VirtualRange::create(process->page_table(), 0, m_interrupt_stack_size, PageTable::Flags::UserSupervisor | PageTable::Flags::ReadWrite | PageTable::Flags::Present));

		thread->setup_exec();

		thread_deleter.disable();

		return thread;
	}

	Thread::Thread(pid_t tid, Process* process)
		: m_tid(tid), m_process(process)
	{}

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
		thread->m_in_syscall = true;

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
		{
			// FIXME: don't use PageTableScope
			PageTableScope _(m_process->page_table());
			write_to_stack(m_rsp, this);
			write_to_stack(m_rsp, &Thread::on_exit);
			write_to_stack(m_rsp, nullptr);
		}
	}

	void Thread::setup_process_cleanup()
	{
		m_state = State::NotStarted;
		static entry_t entry(
			[](void* process)
			{
				((Process*)process)->cleanup_function();
				Scheduler::get().delete_current_process_and_thread();
				ASSERT_NOT_REACHED();				
			}
		);
		m_rsp = stack_base() + stack_size();
		m_rip = (uintptr_t)entry;

		m_signal_mask = ~0ull;

		// Setup stack for returning
		{
			// FIXME: don't use PageTableScope
			PageTableScope _(m_process->page_table());
			write_to_stack(m_rsp, this);
			write_to_stack(m_rsp, &Thread::on_exit);
			write_to_stack(m_rsp, m_process);
		}
	}

	bool Thread::has_signal_to_execute() const
	{
		return !m_signal_queue.empty() && !m_handling_signal;
	}

	void Thread::set_signal_done(int signal)
	{
		ASSERT(!interrupts_enabled());
		if (m_handling_signal == 0)
			derrorln("set_signal_done called while not handling singal");
		if (m_handling_signal != signal)
			derrorln("set_signal_done called with invalid signal");
		else
			m_handling_signal = 0;
	}

	void Thread::handle_next_signal()
	{
		ASSERT(!interrupts_enabled());
		ASSERT(!m_signal_queue.empty());
		ASSERT(&Thread::current() == this);
		ASSERT(is_userspace());

		int signal = m_signal_queue.front();
		ASSERT(signal >= _SIGMIN && signal <= _SIGMAX);
		m_signal_queue.pop();

		uintptr_t& return_rsp = this->return_rsp();
		uintptr_t& return_rip = this->return_rip();

		vaddr_t signal_handler = process().m_signal_handlers[signal];

		// Skip masked and ignored signals
		if (m_signal_mask & (1ull << signal))
			;
		else if (signal_handler == (vaddr_t)SIG_IGN)
			;
		else if (signal_handler != (vaddr_t)SIG_DFL)
		{
			// call userspace signal handlers
			// FIXME: signal trampoline should take a hash etc
			//        to only allow marking signals done from it
			m_handling_signal = signal;
			write_to_stack(return_rsp, return_rip);
			write_to_stack(return_rsp, signal);
			write_to_stack(return_rsp, signal_handler);
			return_rip = (uintptr_t)signal_trampoline;
			return;
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
				{
					auto message = BAN::String::formatted("killed by signal {}\n", signal);
					(void)process().tty().write(0, message.data(), message.size());
				}
				// fall through

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
				{
					process().exit(128 + signal);
					break;
				}

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

	void Thread::queue_signal(int signal)
	{
		ASSERT(!interrupts_enabled());
		if (m_signal_queue.full())
		{
			dwarnln("Signal queue full");
			return;
		}
		m_signal_queue.push(signal);
		if (this != &Thread::current())
			Scheduler::get().unblock_thread(tid());
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
		set_terminating();
		TerminateBlocker(*this);
		ASSERT_NOT_REACHED();
	}

}