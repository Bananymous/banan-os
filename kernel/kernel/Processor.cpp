#include <BAN/Vector.h>
#include <kernel/Processor.h>

namespace Kernel
{

	static BAN::Vector<Processor> s_processors;

	Processor& Processor::create(ProcessorID id)
	{
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
	}

	Processor& Processor::get(ProcessorID id)
	{
		return s_processors[id];
	}

}
