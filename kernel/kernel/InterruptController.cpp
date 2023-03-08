#include <BAN/Errors.h>
#include <kernel/InterruptController.h>
#include <kernel/APIC.h>
#include <kernel/PIC.h>

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
		s_instance = APIC::create();
	if (s_instance)
		return;
	dprintln("Using PIC instead of APIC");
	s_instance = PIC::create();
}

uintptr_t disable_interrupts_and_get_flags()
{
	uintptr_t flags;
	asm volatile("pushf; cli; pop %0" : "=r"(flags) :: "memory");
	return flags;
}

void restore_flags(uintptr_t flags)
{
	asm volatile("push %0; popf" :: "rm"(flags) : "memory", "cc");
}

bool interrupts_enabled()
{
	uintptr_t flags;
	asm volatile("pushf; pop %0" : "=r"(flags) :: "memory");
	return flags & (1 << 9);
}