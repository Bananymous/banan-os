#pragma once

#include <kernel/Input/KeyEvent.h>
#include <kernel/Input/PS2Device.h>
#include <kernel/Input/PS2Keymap.h>
#include <kernel/Semaphore.h>

namespace Kernel::Input
{

	class PS2Keyboard final : public PS2Device
	{
	private:
		enum Command : uint8_t
		{
			SET_LEDS = 0xED,
			SCANCODE = 0xF0
		};

	public:
		static BAN::ErrorOr<PS2Keyboard*> create(PS2Controller&);
		virtual void send_initialize() override;

		virtual void handle_byte(uint8_t) final override;
		virtual void handle_device_command_response(uint8_t) final override;

	private:
		PS2Keyboard(PS2Controller& controller);

		void update_leds();

	private:
		uint8_t m_byte_buffer[10];
		uint8_t m_byte_index { 0 };

		uint8_t m_modifiers { 0 };

		BAN::CircularQueue<KeyEvent, 10> m_event_queue;

		PS2Keymap m_keymap;

		Semaphore m_semaphore;

	protected:
		virtual BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;
		virtual bool has_data_impl() const override;
	};

}