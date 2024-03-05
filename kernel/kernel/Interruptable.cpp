#include <kernel/IDT.h>
#include <kernel/Interruptable.h>
#include <kernel/InterruptController.h>
#include <kernel/Processor.h>

namespace Kernel
{

	void Interruptable::set_irq(int irq)
	{
		auto& processor = Processor::current();
		if (m_irq != -1)
			processor.idt().register_irq_handler(m_irq, nullptr);
		m_irq = irq;
		processor.idt().register_irq_handler(irq, this);
	}

	void Interruptable::enable_interrupt()
	{
		ASSERT(m_irq != -1);
		InterruptController::get().enable_irq(m_irq);
	}

	void Interruptable::disable_interrupt()
	{
		ASSERT_NOT_REACHED();
	}

}
