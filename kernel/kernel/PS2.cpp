#include <kernel/IDT.h>
#include <kernel/PIC.h>
#include <kernel/PS2.h>
#include <kernel/kprint.h>

#define PS2_IRQ 0x01


namespace PS2
{

	void irq_handler()
	{
		kprint("keyboard\n");
	}

	void initialize()
	{
		IDT::register_irq_handler(PS2_IRQ, irq_handler);
		PIC::unmask(PS2_IRQ);
	}

}