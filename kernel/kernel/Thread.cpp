#include <BAN/Errors.h>
#include <kernel/InterruptController.h>
#include <kernel/kmalloc.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Thread.h>

#define PAGE_SIZE 4096

namespace Kernel
{

	static constexpr size_t thread_stack_size = 16384;

	template<size_t size, typename T>
	static void write_to_stack(uintptr_t& rsp, const T& value)
	{
		rsp -= size;
		memcpy((void*)rsp, (void*)&value, size);
	}

	BAN::ErrorOr<Thread*> Thread::create(entry_t entry, void* data, BAN::RefPtr<Process> process)
	{
		static pid_t next_tid = 1;
		auto* thread = new Thread(next_tid++, process);
		if (thread == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		TRY(thread->initialize(entry, data));
		return thread;
	}

	Thread::Thread(pid_t tid, BAN::RefPtr<Process> process)
		: m_tid(tid), m_process(process)
	{}

	Thread& Thread::current()
	{
		return Scheduler::get().current_thread();
	}

	BAN::RefPtr<Process> Thread::process()
	{
		return m_process;
	}

	BAN::ErrorOr<void> Thread::initialize(entry_t entry, void* data)
	{
		m_stack_base = kmalloc(thread_stack_size, PAGE_SIZE);
		if (m_stack_base == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		m_rsp = (uintptr_t)m_stack_base + thread_stack_size;
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

	void Thread::on_exit()
	{
		if (m_process)
			m_process->on_thread_exit(*this);
		Scheduler::get().set_current_thread_done();
		ASSERT_NOT_REACHED();
	}

}