#pragma once

#include <kernel/InterruptController.h>

namespace Kernel
{

	class PIC final : public InterruptController
	{
	public:
		virtual void eoi(uint8_t) override;
		virtual void enable_irq(uint8_t) override;
		virtual bool is_in_service(uint8_t) override;

		static void remap();
		static void mask_all();

	private:
		static PIC* create();
		friend class InterruptController;
	};

}
