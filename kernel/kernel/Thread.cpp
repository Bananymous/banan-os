#include <BAN/Errors.h>
#include <kernel/Arch.h>
#include <kernel/InterruptController.h>
#include <kernel/kmalloc.h>
#include <kernel/Scheduler.h>
#include <kernel/Thread.h>

#define PAGE_SIZE 4096

namespace Kernel
{

	static uint32_t s_next_id = 0;

	static constexpr size_t thread_stack_size = 16384;

	template<size_t size, typename T>
	static void write_to_stack(uintptr_t& rsp, const T& value)
	{
		rsp -= size;
		memcpy((void*)rsp, (void*)&value, size);
	}

	Thread::Thread(uintptr_t rip, uintptr_t func, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3)
		: m_id(s_next_id++)
	{
		m_stack_base = kmalloc(thread_stack_size, PAGE_SIZE);
		ASSERT(m_stack_base);
	
		m_rsp = (uintptr_t)m_stack_base + thread_stack_size;
		m_rip = rip;
		m_args[1] = arg1;
		m_args[2] = arg2;
		m_args[3] = arg3;

		// NOTE: in System V ABI arg0 is the pointer to 'this'
		//       we copy the function object to Thread object
		//       so we can ensure the lifetime of it. We store
		//       it as raw bytes so that Thread can be non-templated.
		//       This requires BAN::Function to be trivially copyable
		//       but for now it should be.
		memcpy(m_function, (void*)func, sizeof(m_function));
		m_args[0] = (uintptr_t)m_function;

		write_to_stack<sizeof(void*)>(m_rsp, this);
		write_to_stack<sizeof(void*)>(m_rsp, &Thread::on_exit);
	}

	Thread::~Thread()
	{
		kfree(m_stack_base);
	}

	void Thread::on_exit()
	{
		asm volatile("cli");
		m_state = State::Done;
		Scheduler::get().switch_thread();
		ASSERT(false);
	}

}