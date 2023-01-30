#pragma once

#include <kernel/InterruptController.h>

class PIC final : public InterruptController
{
public:
	virtual void EOI(uint8_t) override;
	virtual void EnableIrq(uint8_t) override;
	virtual bool IsInService(uint8_t) override;

	static void Remap();
	static void MaskAll();

private:
	static PIC* Create();
	friend class InterruptController;
};