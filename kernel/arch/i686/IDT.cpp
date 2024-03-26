#include <kernel/IDT.h>

namespace Kernel
{

	IDT* IDT::create()
	{
		ASSERT_NOT_REACHED();
	}

	[[noreturn]] void IDT::force_triple_fault()
	{
		ASSERT_NOT_REACHED();
	}

	void IDT::register_irq_handler(uint8_t, Interruptable*)
	{
		ASSERT_NOT_REACHED();
	}

	void IDT::register_interrupt_handler(uint8_t, void (*)())
	{
		ASSERT_NOT_REACHED();
	}

	void IDT::register_syscall_handler(uint8_t, void (*)())
	{
		ASSERT_NOT_REACHED();
	}

}
