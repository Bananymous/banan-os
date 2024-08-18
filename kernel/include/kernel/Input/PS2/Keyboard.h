#pragma once

#include <BAN/Array.h>
#include <kernel/Input/PS2/Device.h>
#include <kernel/Input/PS2/Keymap.h>

namespace Kernel::Input
{

	class PS2Keyboard final : public PS2Device
	{
	private:
		enum Command : uint8_t
		{
			SET_LEDS = 0xED,
			CONFIG_SCANCODE_SET = 0xF0
		};

	public:
		static BAN::ErrorOr<BAN::RefPtr<PS2Keyboard>> create(PS2Controller&, uint8_t scancode_set);
		virtual void send_initialize() override;

		virtual void command_timedout(uint8_t* command_data, uint8_t command_size) final override;

		virtual void handle_byte(uint8_t) final override;

		virtual void update() final override { m_controller.update_command_queue(); }

	private:
		PS2Keyboard(PS2Controller& controller, bool basic);

		void update_leds();

	private:
		BAN::Array<uint8_t, 3> m_byte_buffer;
		uint8_t m_byte_index { 0 };
		bool m_basic { false };

		uint8_t m_scancode_set { 0xFF };
		uint16_t m_modifiers { 0 };

		PS2Keymap m_keymap;

		friend class BAN::RefPtr<PS2Keyboard>;
	};

}
