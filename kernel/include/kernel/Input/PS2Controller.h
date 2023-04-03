#pragma once

#include <kernel/Device.h>

namespace Kernel::Input
{

	class PS2Device : public CharacterDevice
	{
	public:
		virtual ~PS2Device() {}
		virtual void on_byte(uint8_t) = 0;
		
	public:
		virtual Mode mode() const override { return { Mode::IFCHR | Mode::IRUSR | Mode::IRGRP }; }
		virtual uid_t uid() const override { return 0; }
		virtual gid_t gid() const override { return 0; }
	};

	class PS2Controller
	{
	public:
		static BAN::ErrorOr<void> initialize();
		static PS2Controller& get();

		void send_byte(const PS2Device*, uint8_t);

	private:
		PS2Controller() = default;
		BAN::ErrorOr<void> initialize_impl();
		BAN::ErrorOr<void> initialize_device(uint8_t);

		BAN::ErrorOr<void> reset_device(uint8_t);
		BAN::ErrorOr<void> set_scanning(uint8_t, bool);

		static void device0_irq();
		static void device1_irq();

	private:
		PS2Device* m_devices[2] { nullptr, nullptr };
	};

}