#include <BAN/Errors.h>
#include <kernel/InterruptController.h>
#include <kernel/APIC.h>
#include <kernel/PIC.h>

#include <lai/helpers/sci.h>

namespace Kernel
{

	namespace IDT { void register_irq_handler(uint8_t irq, Interruptable*); }

	static InterruptController* s_instance = nullptr;

	void Interruptable::set_irq(int irq)
	{
		if (m_irq != -1)
			IDT::register_irq_handler(m_irq, nullptr);
		m_irq = irq;
		IDT::register_irq_handler(irq, this);
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

	InterruptController& InterruptController::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	void InterruptController::initialize(bool force_pic)
	{
		ASSERT(s_instance == nullptr);

		PIC::mask_all();
		PIC::remap();

		if (!force_pic)
		{
			s_instance = APIC::create();
			if (s_instance)
			{
				s_instance->m_using_apic = true;
				return;
			}
		}

		dprintln("Using PIC instead of APIC");
		s_instance = PIC::create();
		ASSERT(s_instance);

		s_instance->m_using_apic = false;
	}

	void InterruptController::enter_acpi_mode()
	{
		if (lai_enable_acpi(m_using_apic ? 1 : 0) != 0)
			dwarnln("could not enter acpi mode");
	}

	bool interrupts_enabled()
	{
		uintptr_t flags;
		asm volatile("pushf; pop %0" : "=r"(flags) :: "memory");
		return flags & (1 << 9);
	}

}
