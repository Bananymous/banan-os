#include <BAN/Errors.h>
#include <kernel/CriticalScope.h>
#include <kernel/InterruptController.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Thread.h>

namespace Kernel
{

	extern "C" void thread_userspace_trampoline(uint64_t rsp, uint64_t rip, int argc, char** argv);

	template<size_t size, typename T>
	static void write_to_stack(uintptr_t& rsp, const T& value)
	{
		rsp -= size;
		memcpy((void*)rsp, (void*)&value, size);
	}

	static pid_t s_next_tid = 1;

	BAN::ErrorOr<Thread*> Thread::create(entry_t entry, void* data, Process* process)
	{
		// Create the thread object
		Thread* thread = new Thread(s_next_tid++, process);
		if (thread == nullptr)
			return BAN::Error::from_errno(ENOMEM);

		// Initialize stack and registers
		thread->m_stack_base = (vaddr_t)kmalloc(m_kernel_stack_size, PAGE_SIZE);
		if (thread->m_stack_base == 0)
			return BAN::Error::from_errno(ENOMEM);
		thread->m_rsp = (uintptr_t)thread->m_stack_base + m_kernel_stack_size;
		thread->m_rip = (uintptr_t)entry;		

		// Initialize stack for returning
		write_to_stack<sizeof(void*)>(thread->m_rsp, thread);
		write_to_stack<sizeof(void*)>(thread->m_rsp, &Thread::on_exit);
		write_to_stack<sizeof(void*)>(thread->m_rsp, data);

		return thread;
	}

	BAN::ErrorOr<Thread*> Thread::create_userspace(uintptr_t entry, Process* process, int argc, char** argv)
	{
		// Create the thread object
		Thread* thread = new Thread(s_next_tid++, process);
		if (thread == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		thread->m_is_userspace = true;

		// Allocate stack
		thread->m_stack_base = (uintptr_t)kmalloc(m_userspace_stack_size, PAGE_SIZE);
		ASSERT(thread->m_stack_base);
		process->mmu().identity_map_range(thread->m_stack_base, m_userspace_stack_size, MMU::Flags::UserSupervisor | MMU::Flags::ReadWrite | MMU::Flags::Present);

		// Allocate interrupt stack
		thread->m_interrupt_stack = (vaddr_t)kmalloc(m_interrupt_stack_size, PAGE_SIZE);
		ASSERT(thread->m_interrupt_stack);

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
		thread->m_rsp = thread->m_stack_base + m_userspace_stack_size;
		thread->m_rip = (uintptr_t)entry_trampoline;

		// Setup stack for returning
		write_to_stack<sizeof(void*)>(thread->m_rsp, thread);
		write_to_stack<sizeof(void*)>(thread->m_rsp, &Thread::on_exit);
		write_to_stack<sizeof(void*)>(thread->m_rsp, nullptr);

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
		dprintln("thread {} ({}) exit", tid(), m_process->pid());

		if (m_interrupt_stack)
			kfree((void*)m_interrupt_stack);
		kfree((void*)m_stack_base);
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