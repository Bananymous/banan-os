#pragma once

#include <BAN/Optional.h>
#include <BAN/Errors.h>

#include <stdint.h>

namespace Kernel
{

	class Interruptable
	{
	public:
		void set_irq(int irq);
		void enable_interrupt();
		void disable_interrupt();

		virtual void handle_irq() = 0;

	protected:
		Interruptable() = default;
		~Interruptable() {}

	private:
		int m_irq { -1 };
	};

	class InterruptController
	{
	public:
		virtual ~InterruptController() {}

		virtual void eoi(uint8_t) = 0;
		virtual void enable_irq(uint8_t) = 0;
		virtual bool is_in_service(uint8_t) = 0;

		static void initialize(bool force_pic);
		static InterruptController& get();

		virtual void initialize_multiprocessor() = 0;

		virtual BAN::ErrorOr<void> reserve_irq(uint8_t irq) = 0;
		virtual BAN::Optional<uint8_t> get_free_irq() = 0;

		bool is_using_apic() const { return m_using_apic; }

		void enter_acpi_mode();

	private:
		bool m_using_apic { false };
	};

}
