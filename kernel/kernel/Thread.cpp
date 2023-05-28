#include <BAN/Errors.h>
#include <kernel/InterruptController.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Memory/MMUScope.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Thread.h>

namespace Kernel
{

	extern "C" void thread_userspace_trampoline(uint64_t rsp, uint64_t rip, int argc, char** argv);
	extern "C" uintptr_t read_rip();

	template<size_t size, typename T>
	static void write_to_stack(uintptr_t& rsp, const T& value)
	{
		rsp -= size;
		memcpy((void*)rsp, (void*)&value, size);
	}

	static pid_t s_next_tid = 1;

	BAN::ErrorOr<Thread*> Thread::create_kernel(entry_t entry, void* data, Process* process)
	{
		// Create the thread object
		Thread* thread = new Thread(s_next_tid++, process);
		if (thread == nullptr)
			return BAN::Error::from_errno(ENOMEM);

		// Initialize stack and registers
		thread->m_stack = VirtualRange::create_kmalloc(m_kernel_stack_size);
		if (thread->m_stack == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		thread->m_rsp = thread->stack_base() + thread->stack_size();
		thread->m_rip = (uintptr_t)entry;

		// Initialize stack for returning
		write_to_stack<sizeof(void*)>(thread->m_rsp, thread);
		write_to_stack<sizeof(void*)>(thread->m_rsp, &Thread::on_exit);
		write_to_stack<sizeof(void*)>(thread->m_rsp, data);

		return thread;
	}

	BAN::ErrorOr<Thread*> Thread::create_userspace(uintptr_t entry, Process* process, int argc, char** argv)
	{
		ASSERT(process);

		// Create the thread object
		Thread* thread = new Thread(s_next_tid++, process);
		if (thread == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		thread->m_is_userspace = true;

		// Allocate stack
		thread->m_stack = VirtualRange::create(process->mmu(), 0, m_userspace_stack_size, MMU::Flags::UserSupervisor | MMU::Flags::ReadWrite | MMU::Flags::Present);
		if (thread->m_stack == nullptr)
		{
			delete thread;
			return BAN::Error::from_errno(ENOMEM);
		}

		// Allocate interrupt stack
		thread->m_interrupt_stack = VirtualRange::create(process->mmu(), 0, m_interrupt_stack_size, MMU::Flags::UserSupervisor | MMU::Flags::ReadWrite | MMU::Flags::Present);
		if (thread->m_interrupt_stack == nullptr)
		{
			delete thread;
			return BAN::Error::from_errno(ENOMEM);
		}

		thread->m_userspace_entry = { .entry = entry, .argc = argc, .argv = argv };

		// Setup registers and entry
		static entry_t entry_trampoline(
			[](void*)
			{
				userspace_entry_t& entry = Thread::current().m_userspace_entry;
				thread_userspace_trampoline(Thread::current().rsp(), entry.entry, entry.argc, entry.argv);
				ASSERT_NOT_REACHED();
			}
		);
		thread->m_rsp = thread->stack_base() + thread->stack_size();
		thread->m_rip = (uintptr_t)entry_trampoline;

		// Setup stack for returning
		{
			MMUScope _(process->mmu());
			write_to_stack<sizeof(void*)>(thread->m_rsp, thread);
			write_to_stack<sizeof(void*)>(thread->m_rsp, &Thread::on_exit);
			write_to_stack<sizeof(void*)>(thread->m_rsp, nullptr);
		}

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
		if (m_stack)
			delete m_stack;
		m_stack = nullptr;
		if (m_interrupt_stack)
			delete m_interrupt_stack;
		m_interrupt_stack = nullptr;
		
		dprintln("thread {} ({}) exit", tid(), m_process->pid());
	}

	BAN::ErrorOr<Thread*> Thread::clone(Process* new_process, uintptr_t rsp, uintptr_t rip)
	{
		ASSERT(m_is_userspace);
		ASSERT(m_state == State::Executing);

		Thread* thread = new Thread(s_next_tid++, new_process);
		if (thread == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		thread->m_is_userspace = true;

		thread->m_interrupt_stack = m_interrupt_stack->clone(new_process->mmu());
		thread->m_stack = m_stack->clone(new_process->mmu());

		thread->m_state = State::Executing;
		thread->m_in_syscall = true;

		thread->m_rip = rip;
		thread->m_rsp = rsp;

		thread->m_userspace_entry = {};

		return thread;
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
		if (m_process)
			m_process->on_thread_exit(*this);
		Scheduler::get().set_current_thread_done();
		ASSERT_NOT_REACHED();
	}

}