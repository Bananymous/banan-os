#pragma once

#include <stdint.h>

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