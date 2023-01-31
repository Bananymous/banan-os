#include <BAN/Errors.h>
#include <kernel/Arch.h>
#include <kernel/InterruptController.h>
#include <kernel/kmalloc.h>
#include <kernel/Scheduler.h>
#include <kernel/Thread.h>

#define PAGE_SIZE 4096

namespace Kernel
{

	static uint32_t s_next_id = 1;

	static constexpr size_t thread_stack_size = PAGE_SIZE;

	template<typename T>
	static void write_to_stack(uintptr_t& rsp, const T& value)
	{
		rsp -= sizeof(T);
		*(T*)rsp = value;
	}

	Thread::Thread(void(*function)())
		: m_id(s_next_id++)
	{
		m_stack_base = kmalloc(thread_stack_size, PAGE_SIZE);
		ASSERT(m_stack_base);

		m_rip = (uintptr_t)function;
		m_rsp = (uintptr_t)m_stack_base + thread_stack_size;
		write_to_stack(m_rsp, this);
		write_to_stack(m_rsp, &Thread::on_exit);
	}

	Thread::~Thread()
	{
		kfree(m_stack_base);
	}

	void Thread::on_exit()
	{
		Thread* thread = nullptr;
#if ARCH(x86_64)
		asm volatile("movq (%%rsp), %0" : "=r"(thread));
#else
		asm volatile("movl (%%esp), %0" : "=r"(thread));
#endif
		thread->m_state = State::Done;
		for (;;) asm volatile("hlt");
	}

}