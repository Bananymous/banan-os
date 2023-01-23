#include <BAN/Errors.h>
#include <kernel/InterruptController.h>
#include <kernel/APIC.h>
#include <kernel/PIC.h>

static InterruptController* s_instance = nullptr;

InterruptController& InterruptController::Get()
{
	ASSERT(s_instance);
	return *s_instance;
}

void InterruptController::Initialize(bool force_pic)
{
	ASSERT(s_instance == nullptr);

	PIC::MaskAll();
	PIC::Remap();

	if (!force_pic)
		s_instance = APIC::Create();
	if (s_instance)
		return;
	dprintln("Using PIC instead of APIC");
	s_instance = PIC::Create();
}