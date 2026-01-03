#pragma once

#include <kernel/USB/HID/HIDDriver.h>

#include <LibInput/Joystick.h>

namespace Kernel
{

	class USBJoystick final : public USBHIDDevice
	{
		BAN_NON_COPYABLE(USBJoystick);
		BAN_NON_MOVABLE(USBJoystick);

	public:
		BAN::ErrorOr<void> initialize() override;

		void start_report() override;
		void stop_report() override;

		void handle_array(uint16_t usage_page, uint16_t usage) override;
		void handle_variable(uint16_t usage_page, uint16_t usage, int64_t state) override;
		void handle_variable_absolute(uint16_t usage_page, uint16_t usage, int64_t state, int64_t min, int64_t max) override;

	protected:
		BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;
		bool can_read_impl() const override { return true; }

	private:
		USBJoystick(USBHIDDriver&);
		~USBJoystick() = default;

	private:
		USBHIDDriver& m_driver;

		SpinLock m_state_lock;
		InterruptState m_interrupt_state;
		LibInput::JoystickState m_state {};

		friend class BAN::RefPtr<USBJoystick>;
	};

}
