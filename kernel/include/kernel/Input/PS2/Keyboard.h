#pragma once

#include <BAN/Array.h>
#include <BAN/CircularQueue.h>
#include <kernel/Input/KeyEvent.h>
#include <kernel/Input/PS2/Device.h>
#include <kernel/Input/PS2/Keymap.h>
#include <kernel/Semaphore.h>

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
		static BAN::ErrorOr<PS2Keyboard*> create(PS2Controller&);
		virtual void send_initialize() override;

		virtual void handle_byte(uint8_t) final override;

	private:
		PS2Keyboard(PS2Controller& controller);

		void update_leds();

	private:
		BAN::Array<uint8_t, 3> m_byte_buffer;
		uint8_t m_byte_index { 0 };

		uint8_t m_scancode_set { 0xFF };

		uint16_t m_modifiers { 0 };

		BAN::CircularQueue<KeyEvent, 50> m_event_queue;

		PS2Keymap m_keymap;

		Semaphore m_semaphore;

	protected:
		virtual BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;
		virtual bool has_data_impl() const override;
	};

}
