#pragma once

#include <BAN/CircularQueue.h>
#include <kernel/Input/KeyEvent.h>
#include <kernel/Input/PS2Controller.h>
#include <kernel/Input/PS2Keymap.h>

namespace Kernel::Input
{

	class PS2Keyboard final : public PS2Device
	{
	private:
		enum Command : uint8_t
		{
			SET_LEDS = 0xED,
			SCANCODE = 0xF0,
			ENABLE_SCANNING = 0xF4,
			DISABLE_SCANNING = 0xF5,
		};

		enum class State
		{
			Normal,
			WaitingAck,
		};

	public:
		static BAN::ErrorOr<PS2Keyboard*> create(PS2Controller&, dev_t);

		virtual void on_byte(uint8_t) override;
		virtual void update() override;

	private:
		PS2Keyboard(PS2Controller& controller, dev_t device)
			: PS2Device(device)
			, m_controller(controller)
			, m_name(BAN::String::formatted("input{}", device))
		{}
		BAN::ErrorOr<void> initialize();

		void append_command_queue(uint8_t);
		void append_command_queue(uint8_t, uint8_t);

		void buffer_has_key();

		void update_leds();

	private:
		PS2Controller& m_controller;
		uint8_t m_byte_buffer[10];
		uint8_t m_byte_index { 0 };

		uint8_t m_modifiers { 0 };

		BAN::CircularQueue<KeyEvent, 10> m_event_queue;
		BAN::CircularQueue<uint8_t, 10> m_command_queue;

		PS2Keymap m_keymap;

		State m_state { State::Normal };

		BAN::String m_name;

	public:
		virtual BAN::StringView name() const override { return m_name; }
		virtual blksize_t blksize() const override { return sizeof(KeyEvent); }
		virtual dev_t rdev() const override { return 0x8594; }
		virtual BAN::ErrorOr<size_t> read(size_t, void*, size_t) override;
	};

}