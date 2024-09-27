#include <kernel/IDT.h>
#include <kernel/Interruptable.h>
#include <kernel/InterruptController.h>
#include <kernel/Processor.h>

namespace Kernel
{

	void Interruptable::set_irq(int irq)
	{
		if (m_irq != -1)
			Processor::idt().register_irq_handler(m_irq, nullptr);
		m_irq = irq;
		Processor::idt().register_irq_handler(irq, this);
	}

}
