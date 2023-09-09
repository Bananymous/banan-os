#pragma once

#include <BAN/CircularQueue.h>
#include <kernel/Input/KeyEvent.h>
#include <kernel/Input/PS2Controller.h>
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
		static BAN::ErrorOr<PS2Keyboard*> create(PS2Controller&);

		virtual void on_byte(uint8_t) override;
		virtual void update() override;

	private:
		PS2Keyboard(PS2Controller& controller);
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

		Semaphore m_semaphore;

	public:
		virtual dev_t rdev() const override { return m_rdev; }

	protected:
		virtual BAN::ErrorOr<size_t> read_impl(off_t, void*, size_t) override;

	private:
		const dev_t m_rdev;
	};

}