#include <BAN/Errors.h>
#include <kernel/InterruptController.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Thread.h>

#define PAGE_SIZE 4096

namespace Kernel
{

	extern "C" void thread_jump_userspace(uintptr_t rsp, uintptr_t rip);

	template<size_t size, typename T>
	static void write_to_stack(uintptr_t& rsp, const T& value)
	{
		rsp -= size;
		memcpy((void*)rsp, (void*)&value, size);
	}

	BAN::ErrorOr<Thread*> Thread::create(entry_t entry, void* data, Process* process)
	{
		static pid_t next_tid = 1;
		auto* thread = new Thread(next_tid++, process);
		if (thread == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		TRY(thread->initialize(entry, data));
		return thread;
	}

	BAN::ErrorOr<Thread*> Thread::create_userspace(uintptr_t entry, Process* process)
	{
		Thread* thread = TRY(Thread::create(
			[](void* entry)
			{
				Thread::current().jump_userspace((uintptr_t)entry);
				ASSERT_NOT_REACHED();
			}, (void*)entry, process
		));
		process->mmu().map_range(thread->stack_base(), thread->stack_size(), MMU::Flags::UserSupervisor | MMU::Flags::ReadWrite | MMU::Flags::Present);
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

	BAN::ErrorOr<void> Thread::initialize(entry_t entry, void* data)
	{
		m_stack_base = kmalloc(m_stack_size, PAGE_SIZE);
		if (m_stack_base == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		m_rsp = (uintptr_t)m_stack_base + m_stack_size;
		m_rip = (uintptr_t)entry;

		write_to_stack<sizeof(void*)>(m_rsp, this);
		write_to_stack<sizeof(void*)>(m_rsp, &Thread::on_exit);
		write_to_stack<sizeof(void*)>(m_rsp, data);

		return {};
	}

	Thread::~Thread()
	{
		dprintln("thread {} exit", tid());
		kfree(m_stack_base);
	}

	void Thread::jump_userspace(uintptr_t rip)
	{
		thread_jump_userspace(rsp(), rip);
	}

	void Thread::on_exit()
	{
		if (m_process)
			m_process->on_thread_exit(*this);
		Scheduler::get().set_current_thread_done();
		ASSERT_NOT_REACHED();
	}

}