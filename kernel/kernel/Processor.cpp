#include <kernel/Memory/kmalloc.h>
#include <kernel/Processor.h>
#include <kernel/Thread.h>

namespace Kernel
{

	static constexpr uint32_t MSR_IA32_GS_BASE = 0xC0000101;

	ProcessorID Processor::s_bsb_id { PROCESSOR_NONE };

	static BAN::Array<Processor, 0xFF> s_processors;

	static ProcessorID read_processor_id()
	{
		uint32_t id;
		asm volatile(
			"movl $1, %%eax;"
			"cpuid;"
			"shrl $24, %%ebx;"
			: "=b"(id)
			:: "eax", "ecx", "edx"
		);
		return id;
	}

	Processor& Processor::create(ProcessorID id)
	{
		// bsb is the first processor
		if (s_bsb_id == PROCESSOR_NONE)
			s_bsb_id = id = read_processor_id();

		auto& processor = s_processors[id];

		ASSERT(processor.m_id == PROCESSOR_NONE);
		processor.m_id = id;

		processor.m_stack = kmalloc(s_stack_size, 4096, true);
		ASSERT(processor.m_stack);

		processor.m_gdt = GDT::create(&processor);
		ASSERT(processor.m_gdt);

		processor.m_idt = IDT::create();
		ASSERT(processor.m_idt);

		return processor;
	}

	Processor& Processor::initialize()
	{
		auto id = read_processor_id();
		auto& processor = s_processors[id];

		ASSERT(processor.m_gdt);
		processor.m_gdt->load();

		// initialize GS
#if ARCH(x86_64)
		// set gs base to pointer to this processor
		uint64_t ptr = reinterpret_cast<uint64_t>(&processor);
		uint32_t ptr_hi = ptr >> 32;
		uint32_t ptr_lo = ptr & 0xFFFFFFFF;
		asm volatile("wrmsr" :: "d"(ptr_hi), "a"(ptr_lo), "c"(MSR_IA32_GS_BASE));
#elif ARCH(i686)
		asm volatile("movw $0x28, %%ax; movw %%ax, %%gs" ::: "ax");
#endif

		ASSERT(processor.m_idt);
		processor.idt().load();

		return processor;
	}

	void Processor::allocate_idle_thread()
	{
		ASSERT(idle_thread() == nullptr);
		auto* idle_thread = MUST(Thread::create_kernel([](void*) { for (;;) asm volatile("hlt"); }, nullptr, nullptr));
		write_gs_ptr(offsetof(Processor, m_idle_thread), idle_thread);
	}

}
