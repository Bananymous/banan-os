#pragma once

#include <kernel/USB/HID/HIDDriver.h>

namespace Kernel
{

	class USBMouse final : public USBHIDDevice
	{
		BAN_NON_COPYABLE(USBMouse);
		BAN_NON_MOVABLE(USBMouse);

	public:
		void start_report() override;
		void stop_report() override;

		void handle_variable(uint16_t usage_page, uint16_t usage, int64_t state) override;
		void handle_array(uint16_t usage_page, uint16_t usage) override;

	private:
		USBMouse()
			: USBHIDDevice(InputDevice::Type::Mouse)
		{}
		~USBMouse() = default;

	private:
		BAN::Array<bool, 5> m_button_state      { false };
		BAN::Array<bool, 5> m_button_state_temp { false };
		int64_t m_pointer_x { 0 };
		int64_t m_pointer_y { 0 };
		int64_t m_wheel { 0 };

		friend class BAN::RefPtr<USBMouse>;
	};

}
