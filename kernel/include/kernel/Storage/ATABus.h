#pragma once

#include <BAN/Errors.h>
#include <kernel/InterruptController.h>
#include <kernel/SpinLock.h>
#include <kernel/Storage/ATAController.h>

namespace Kernel
{

	class ATADevice;

	class ATABus : public Interruptable
	{
	public:
		enum class DeviceType
		{
			None,
			ATA,
			ATAPI,
		};

	public:
		static ATABus* create(ATAController&, uint16_t base, uint16_t ctrl, uint8_t irq);

		BAN::ErrorOr<void> read(ATADevice&, uint64_t, uint8_t, uint8_t*);
		BAN::ErrorOr<void> write(ATADevice&, uint64_t, uint8_t, const uint8_t*);

		ATAController& controller() { return m_controller; }

		virtual void handle_irq() override;

	private:
		ATABus(ATAController& controller, uint16_t base, uint16_t ctrl)
			: m_controller(controller)
			, m_base(base)
			, m_ctrl(ctrl)
		{}
		void initialize(uint8_t irq);

		void select_device(const ATADevice&);
		DeviceType identify(const ATADevice&, uint16_t*);

		void block_until_irq();
		uint8_t device_index(const ATADevice&) const;

		uint8_t io_read(uint16_t);
		void io_write(uint16_t, uint8_t);
		void read_buffer(uint16_t, uint16_t*, size_t);
		void write_buffer(uint16_t, const uint16_t*, size_t);
		BAN::ErrorOr<void> wait(bool);
		BAN::Error error();

	private:
		ATAController& m_controller;
		const uint16_t m_base;
		const uint16_t m_ctrl;
		SpinLock m_lock;

		bool m_has_got_irq { false };

		BAN::RefPtr<ATADevice> m_devices[2] {};

		friend class ATAController;
	};

}