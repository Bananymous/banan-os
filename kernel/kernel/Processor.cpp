#include <BAN/Vector.h>
#include <kernel/Processor.h>

namespace Kernel
{

	ProcessorID Processor::s_bsb_id { PROCESSOR_NONE };

	static BAN::Vector<Processor> s_processors;

	Processor& Processor::create(ProcessorID id)
	{
		// bsb is the first processor
		if (s_bsb_id == PROCESSOR_NONE)
			s_bsb_id = id;

		while (s_processors.size() <= id)
			MUST(s_processors.emplace_back());
		auto& processor = s_processors[id];
		if (processor.m_stack == nullptr)
		{
			processor.m_stack = kmalloc(m_stack_size, 4096, true);
			ASSERT(processor.m_stack);
		}
		return processor;
	}

	Processor::~Processor()
	{
		if (m_stack)
			kfree(m_stack);
		m_stack = nullptr;

		if (m_gdt)
			delete m_gdt;
		m_gdt = nullptr;
	}

	Processor& Processor::get(ProcessorID id)
	{
		return s_processors[id];
	}

	void Processor::initialize()
	{
		ASSERT(this == &Processor::current());

		ASSERT(m_gdt == nullptr);
		m_gdt = GDT::create();
		ASSERT(m_gdt);
	}

}
