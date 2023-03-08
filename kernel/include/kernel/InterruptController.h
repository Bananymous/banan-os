#pragma once

#include <stdint.h>

#define DISABLE_INTERRUPTS() asm volatile("cli")
#define ENABLE_INTERRUPTS() asm volatile("sti")

class InterruptController
{
public:
	virtual ~InterruptController() {}

	virtual void eoi(uint8_t) = 0;
	virtual void enable_irq(uint8_t) = 0;
	virtual bool is_in_service(uint8_t) = 0;

	static void initialize(bool force_pic);
	static InterruptController& get();
};

uintptr_t disable_interrupts_and_get_flags();
void restore_flags(uintptr_t);
bool interrupts_enabled();