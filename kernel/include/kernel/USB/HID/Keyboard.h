#pragma once

#include <kernel/USB/HID/HIDDriver.h>

namespace Kernel
{

	class USBKeyboard final : public USBHIDDevice
	{
		BAN_NON_COPYABLE(USBKeyboard);
		BAN_NON_MOVABLE(USBKeyboard);

	public:
		void start_report() override;
		void stop_report() override;

		void handle_variable(uint16_t usage_page, uint16_t usage, int64_t state) override;
		void handle_array(uint16_t usage_page, uint16_t usage) override;

		void update() override;

	private:
		USBKeyboard()
			: USBHIDDevice(InputDevice::Type::Keyboard)
		{}
		~USBKeyboard() = default;

	private:
		SpinLock m_keyboard_lock;
		InterruptState m_lock_state;

		BAN::Array<bool, 0x100> m_keyboard_state      { false };
		BAN::Array<bool, 0x100> m_keyboard_state_temp { false };
		uint8_t m_toggle_mask { 0 };

		BAN::Optional<uint8_t> m_repeat_scancode;
		uint8_t m_repeat_modifier { 0 };
		uint64_t m_next_repeat_event_ms { 0 };

		friend class BAN::RefPtr<USBKeyboard>;
	};

}
