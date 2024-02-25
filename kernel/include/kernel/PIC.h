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

		virtual BAN::ErrorOr<void> reserve_irq(uint8_t irq) override;
		virtual BAN::Optional<uint8_t> get_free_irq() override;

		static void remap();
		static void mask_all();

	private:
		static PIC* create();

	private:
		SpinLock m_lock;
		uint16_t m_reserved_irqs { 0 };
		friend class InterruptController;
	};

}
