#include <kernel/Memory/kmalloc.h>
#include <kernel/Processor.h>

#include <kernel/Debug.h>

namespace Kernel
{

	static constexpr uint32_t MSR_IA32_GS_BASE = 0xC0000101;

	ProcessorID Processor::s_bsb_id { PROCESSOR_NONE };

	static BAN::Array<Processor, 0xFF> s_processors;

	static ProcessorID read_processor_id()
	{
		uint8_t id;
		asm volatile(
			"movl $1, %%eax;"
			"cpuid;"
			"shrl $24, %%ebx;"
			"movb %%bl, %0;"
			: "=rm"(id)
			:: "eax", "ebx", "ecx", "edx"
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

		processor.m_gdt = GDT::create();
		ASSERT(processor.m_gdt);

		processor.m_idt = IDT::create(id == s_bsb_id);
		ASSERT(processor.m_idt);

		return processor;
	}

	Processor& Processor::initialize()
	{
		auto id = read_processor_id();
		auto& processor = s_processors[id];

		// set gs base to pointer to this processor
		uint64_t ptr = reinterpret_cast<uint64_t>(&processor);
		asm volatile("wrmsr" :: "d"(ptr >> 32), "a"(ptr), "c"(MSR_IA32_GS_BASE));

		ASSERT(processor.m_gdt);
		processor.gdt().load();

		ASSERT(processor.m_idt);
		processor.idt().load();

		return processor;
	}

}
