#include <BAN/Errors.h>
#include <kernel/InterruptController.h>
#include <kernel/APIC.h>
#include <kernel/PIC.h>

#include <lai/helpers/sci.h>

namespace Kernel
{

	static InterruptController* s_instance = nullptr;

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

}
