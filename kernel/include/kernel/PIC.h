#pragma once

#include <kernel/InterruptController.h>

class PIC final : public InterruptController
{
public:
	virtual void EOI(uint8_t) override;
	virtual void EnableIrq(uint8_t) override;
	virtual void GetISR(uint32_t[8]) override;

	static void Remap();
	static void MaskAll();

private:
	static PIC* Create();
	friend class InterruptController;
};