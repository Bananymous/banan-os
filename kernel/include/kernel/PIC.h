#pragma once

#include <kernel/InterruptController.h>
#include <kernel/Lock/SpinLock.h>

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

		virtual void initialize_multiprocessor() override;
		virtual void send_ipi(ProcessorID) override {}
		virtual void broadcast_ipi() override {}
		virtual void enable() override {}

		static void remap();
		static void mask_all();

	private:
		static PIC* create();

	private:
		SpinLock m_lock;
		uint16_t m_reserved_irqs { 1u << 2 };

		friend class InterruptController;
	};

}
