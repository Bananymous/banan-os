#pragma once

#include <kernel/USB/HID/HIDDriver.h>

#include <LibInput/Joystick.h>

namespace Kernel
{

	class USBJoystick final : public USBHIDDevice
	{
		BAN_NON_COPYABLE(USBJoystick);
		BAN_NON_MOVABLE(USBJoystick);

		enum class Type
		{
			Unknown,
			DualShock3,
		};

	public:
		BAN::ErrorOr<void> initialize() override;

		void start_report() override;
		void stop_report() override;

		void handle_array(uint16_t usage_page, uint16_t usage) override;
		void handle_variable(uint16_t usage_page, uint16_t usage, int64_t state) override;
		void handle_variable_absolute(uint16_t usage_page, uint16_t usage, int64_t state, int64_t min, int64_t max) override;

		void update() override;

	protected:
		BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;
		bool can_read_impl() const override { return true; }

		BAN::ErrorOr<long> ioctl_impl(int request, void* arg) override;

	private:
		USBJoystick(USBHIDDriver&);
		~USBJoystick() = default;

	private:
		void initialize_type();

		BAN::ErrorOr<void> update_dualshock3_state(uint8_t led_bitmap, uint8_t rumble_strength);

	private:
		USBHIDDriver& m_driver;

		Type m_type { Type::Unknown };

		BAN::UniqPtr<DMARegion> m_send_buffer;

		SpinLock m_state_lock;
		InterruptState m_interrupt_state;
		LibInput::JoystickState m_state;
		size_t m_state_index { 0 };

		BAN::Atomic<bool> m_has_got_report { false };

		Mutex m_command_mutex;

		BAN::Atomic<bool> m_has_initialized_leds { false };
		uint8_t m_led_state       { 0b0001 };
		uint8_t m_rumble_strength { 0x00 };

		friend class BAN::RefPtr<USBJoystick>;
	};

}
