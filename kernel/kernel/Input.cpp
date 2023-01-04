#include <BAN/Queue.h>
#include <kernel/APIC.h>
#include <kernel/IDT.h>
#include <kernel/Input.h>
#include <kernel/IO.h>
#include <kernel/kprint.h>
#include <kernel/PIT.h>
#include <kernel/Serial.h>

#include <kernel/KeyboardLayout/FI.h>

#define DEBUG_ALL_IRQ 0
#define KEYBOARD_SHOW_UNKNOWN 1

#define MOUSE_ENABLED 0

#define I8042_DATA_PORT				0x60
#define I8042_STATUS_REGISTER		0x64
#define I8042_COMMAND_REGISTER		0x64

#define I8042_STATUS_OUT_FULL		(1 << 0)
#define I8042_STATUS_IN_FULL		(1 << 1)

#define I8042_READ_CONFIG			0x20
#define I8042_WRITE_CONFIG			0x60

#define I8042_CONFING_IRQ_FIRST		(1 << 0)
#define I8042_CONFING_IRQ_SECOND	(1 << 1)
#define I8042_CONFING_DUAL_CHANNEL	(1 << 5)
#define I8042_CONFING_TRANSLATION	(1 << 6)

#define I8042_ENABLE_FIRST_PORT		0xAE
#define I8042_ENABLE_SECOND_PORT	0xA8
#define I8042_DISABLE_FIRST_PORT	0xAD
#define I8042_DISABLE_SECOND_PORT	0xA7

#define I8042_TEST_CONTROLLER		0xAA
#define I8042_TEST_CONTROLLER_PASS	0x55

#define I8042_TEST_FIRST_PORT		0xAB
#define I8042_TEST_FIRST_PORT_PASS	0x00

#define I8042_TEST_SECOND_PORT		0xA9
#define I8042_TEST_SECOND_PORT_PASS 0x00

#define I8042_KB_ACK				0xFA
#define I8042_KB_RESEND				0xFE
#define I8042_KB_RESET				0xFF
#define I8042_KB_SELF_TEST_PASS		0xAA
#define I8042_KB_SET_SCAN_CODE_SET	0xF0
#define I8042_KB_SET_LEDS			0xED
#define I8042_KB_LED_SCROLL_LOCK	(1 << 0)
#define I8042_KB_LED_NUM_LOCK		(1 << 1)
#define I8042_KB_LED_CAPS_LOCK		(1 << 2)

#define I8042_MOUSE_ACK				0xFA
#define I8042_MOUSE_RESET			0xFF
#define I8042_MOUSE_SELF_TEST_PASS	0xAA
#define I8042_MOUSE_ENABLE			0xF4
#define I8042_MOUSE_DISABLE			0xF5

#define I8042_TIMEOUT_MS			1000

#define KEYBOARD_IRQ				0x01
#define MOUSE_IRQ					0x0C

#define TARGET_KEYBOARD				1
#define TARGET_MOUSE				2

#define MOD_ALT		(1 << 0)
#define MOD_CTRL	(1 << 1)
#define MOD_SHIFT	(1 << 2)
#define MOD_ALTGR	(1 << 3)
#define MOD_CAPS	(1 << 4)

namespace Input
{

	static bool s_keyboard_state[0xFF] = {};

	struct Command
	{
		uint8_t target		= 0;
		uint8_t	command		= 0;
		uint8_t	data		= 0;
		bool	has_data	= false;
		uint8_t	resp_cnt	= 0;
		uint8_t	_sent		= 0;
		uint8_t _ack		= 0;
		bool	_done		= false;
	};
	static uint64_t				s_command_sent = 0;
	static BAN::Queue<Command>	s_command_queue;
	static uint8_t				s_command_response[3] = {};
	static uint8_t				s_command_response_index = 0;

	static BAN::Queue<KeyEvent> s_key_event_queue;
	static uint8_t s_keyboard_key_buffer[10] = {};
	static uint8_t s_keyboard_key_buffer_size = 0;

	static BAN::Queue<MouseButtonEvent> s_mouse_button_event_queue;
	static BAN::Queue<MouseMoveEvent> s_mouse_move_event_queue;
	static uint8_t s_mouse_data_buffer[3] = {};
	static uint8_t s_mouse_data_buffer_index = 0;

	static uint8_t s_led_states	= 0b000;
	static uint8_t s_modifiers	= 0x00;

	static void (*s_key_event_callback)(KeyEvent) = nullptr;
	static void (*s_mouse_button_event_callback)(MouseButtonEvent) = nullptr;
	static void (*s_mouse_move_event_callback)(MouseMoveEvent) = nullptr;

	static const char* s_key_to_utf8_lower[]
	{
		nullptr, nullptr,
		"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
		"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
		"å", "ä", "ö",

		",", ":", ".", ";", "-", "_", "'", "*", "^", "~",
		"!", "?", "\"", "#", "%", "&", "/", "\\", "+", "=",
		"(", ")", "[", "]", "{", "}",
		"$", "£", "€", "¤", "\n", " ", "\t", nullptr, "<", ">", "´", "`", "§", "½", "@", "|",
		nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

		nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

		"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
		",", "+", "*", "/", "-", "\n",

		nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
 
		nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	};
	static_assert(sizeof(s_key_to_utf8_lower) == (int)Key::Count * sizeof(*s_key_to_utf8_lower));

	static const char* s_key_to_utf8_upper[]
	{
		nullptr, nullptr,
		"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
		"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
		"Å", "Ä", "Ö",

		",", ":", ".", ";", "-", "_", "'", "*", "^", "~",
		"!", "?", "\"", "#", "%", "&", "/", "\\", "+", "=",
		"(", ")", "[", "]", "{", "}",
		"$", "£", "€", "¤", "\n", " ", "\t", nullptr, "<", ">", "´", "`", "§", "½", "@", "|",
		nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

		nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

		"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
		",", "+", "*", "/", "-", "\n",

		nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
 
		nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	};
	static_assert(sizeof(s_key_to_utf8_upper) == (int)Key::Count * sizeof(*s_key_to_utf8_upper));



	static void keyboard_new_key();


	static uint8_t wait_and_read()
	{
		while ((IO::inb(I8042_STATUS_REGISTER) & I8042_STATUS_OUT_FULL) == 0)
			continue;
		return IO::inb(I8042_DATA_PORT);
	}

	static void i8042_controller_command(uint8_t command)
	{
		IO::outb(I8042_COMMAND_REGISTER, command);
	}

	static void i8042_controller_command(uint8_t command, uint8_t data)
	{
		IO::outb(I8042_COMMAND_REGISTER, command);
		while ((IO::inb(I8042_STATUS_REGISTER) & I8042_STATUS_IN_FULL) != 0)
			continue;
		IO::outb(I8042_DATA_PORT, data);
	}

	static bool i8042_command(uint8_t target, uint8_t command)
	{
		if (target == TARGET_MOUSE)
			IO::outb(I8042_COMMAND_REGISTER, 0xD4);	

		auto timeout = PIT::ms_since_boot() + I8042_TIMEOUT_MS;

		while (PIT::ms_since_boot() < timeout)
		{
			if ((IO::inb(I8042_STATUS_REGISTER) & I8042_STATUS_IN_FULL) == 0)
			{
				IO::outb(I8042_DATA_PORT, command);
				s_command_sent = PIT::ms_since_boot();
				return true;
			}
		}

		return false;
	}

	static void i8042_handle_byte(uint8_t target, uint8_t raw)
	{
		bool waiting_response = false;

		if (!s_command_queue.Empty())
		{
			auto& command = s_command_queue.Front();
			if (command.target == target && command._sent && !command._done)
				waiting_response = true;
		}

		if (target == TARGET_KEYBOARD)
		{
			if (waiting_response)
			{
				auto& command = s_command_queue.Front();

				if (raw == I8042_KB_RESEND)
				{
					dprintln("PS/2 Keyboard: Resend 0x{H}", command._sent == 2 ? command.data : command.command);
					command._sent--;
				}
				else if (raw == I8042_KB_ACK)
				{
					command._ack++;
					if (command.resp_cnt == 0)
						command._done = (command._ack >= (1 + command.has_data));
				}
				else if (raw == 0x00)
				{
					dprintln("\e[33mKey detection error or internal buffer overrun\e[m");
					command._sent = 0;
					command._ack = 0;
					command._done = false;
					s_command_response_index = 0;
				}
				else if (raw == 0xEE && command.command == 0xEE)
				{
					s_command_queue.Pop();
				}
				else
				{
					s_command_response[s_command_response_index++] = raw;
					if (s_command_response_index >= command.resp_cnt)
						command._done = true;
				}
			}
			else
			{
				s_keyboard_key_buffer[s_keyboard_key_buffer_size++] = raw;
				if (raw != 0xE0)
					keyboard_new_key();
			}
		}
		else if (target == TARGET_MOUSE)
		{
			if (waiting_response)
			{
				auto& command = s_command_queue.Front();

				if (raw == I8042_MOUSE_ACK)
				{
					command._ack++;
					if (command.resp_cnt == 0)
						command._done = (command._ack >= (1 + command.has_data));
				}
				else
				{
					s_command_response[s_command_response_index++] = raw;
					if (s_command_response_index >= command.resp_cnt)
						command._done = true;
				}
			}
			else
			{
				s_mouse_data_buffer[s_mouse_data_buffer_index++] = raw;

				if (s_mouse_data_buffer_index >= 3)
				{
					if (s_mouse_data_buffer[0] & 0x07)
					{
						bool left   = s_mouse_data_buffer[0] & (1 << 0);
						bool right  = s_mouse_data_buffer[0] & (1 << 1);
						bool middle = s_mouse_data_buffer[0] & (1 << 2);

						if (left)	s_mouse_button_event_queue.Push({ .button = MouseButton::Left });
						if (right)	s_mouse_button_event_queue.Push({ .button = MouseButton::Right });
						if (middle)	s_mouse_button_event_queue.Push({ .button = MouseButton::Middle });
					}

					if (s_mouse_data_buffer[1] || s_mouse_data_buffer[2])
					{
						int16_t rel_x = (int16_t)s_mouse_data_buffer[1] - ((s_mouse_data_buffer[0] << 4) & 0x100);
						int16_t rel_y = (int16_t)s_mouse_data_buffer[2] - ((s_mouse_data_buffer[0] << 3) & 0x100);

						s_mouse_move_event_queue.Push({ .dx = rel_x, .dy = rel_y }); 
					}		

					s_mouse_data_buffer_index = 0;
				}
			}
		}
		else
		{
			Kernel::panic("Unknown target");
		}
	}

	void update()
	{
		if (!s_command_queue.Empty())
		{
			auto& command = s_command_queue.Front();
			if (command.target != TARGET_KEYBOARD && command.target != TARGET_MOUSE)
				Kernel::panic("Undefined target for command 0x{2H}", command.command);

			if (command._sent == 0 && command._ack == 0)
			{
				command._sent++;
				if (!i8042_command(command.target, command.command))
					Kernel::panic("PS/2 command oof {}, 0x{2H}", command.target, command.command);
			}

			if (command._sent == 1 && command._ack == 1 && command.has_data)
			{
				command._sent++;
				if (!i8042_command(command.target, command.data))
					Kernel::panic("PS/2 data oof {}, 0x{2H}", command.target, command.data);
			}
			
			if (command._sent > 0 && PIT::ms_since_boot() > s_command_sent + 1000)
			{
				kprintln("PS/2 command 0x{2H} timed out on {}", command.command, command.target);
				// Discard command on timeout? 
				command._done = true;
				command.target = 0;
			}

			if (command._done)
			{
				if (command.target == TARGET_KEYBOARD)
				{
					switch (command.command)
					{
						case I8042_KB_RESET:
							if (s_command_response[0] != I8042_KB_SELF_TEST_PASS)
								Kernel::panic("PS/2 Keyboard self test failed");
							break;
						case I8042_KB_SET_SCAN_CODE_SET:
							break;
						case I8042_KB_SET_LEDS:
							break;
						default:
							Kernel::panic("PS/2 Keyboard unhandled command");
					}
				}
				else if (command.target == TARGET_MOUSE)
				{
					switch (command.command)
					{
						case I8042_MOUSE_RESET:
							if (s_command_response[0] != I8042_MOUSE_SELF_TEST_PASS)
								Kernel::panic("PS/2 Mouse self test failed");
							if (s_command_response[1] != 0x00)
								Kernel::panic("PS/2 Mouse invalid byte sent after self test");
							break;
						case I8042_MOUSE_ENABLE:
							break;
						case I8042_MOUSE_DISABLE:
							break;
						default:
							Kernel::panic("PS/2 Mouse unhandled command");
					}
				}

				s_command_response_index = 0;
				s_command_queue.Pop();
			}
		}

		while (!s_key_event_queue.Empty())
		{
			if (s_key_event_callback)
				s_key_event_callback(s_key_event_queue.Front());
			s_key_event_queue.Pop();
		}

		while (!s_mouse_button_event_queue.Empty())
		{
			if (s_mouse_button_event_callback)
				s_mouse_button_event_callback(s_mouse_button_event_queue.Front());
			s_mouse_button_event_queue.Pop();
		}

		while (!s_mouse_move_event_queue.Empty())
		{
			if (s_mouse_move_event_callback)
				s_mouse_move_event_callback(s_mouse_move_event_queue.Front());
			s_mouse_move_event_queue.Pop();
		}
	}

	bool is_key_down(Key key)
	{
		return s_keyboard_state[(int)key];
	}

	static void keyboard_new_key()
	{
		uint8_t index = 0;
		bool extended = (s_keyboard_key_buffer[index] == 0xE0);
		if (extended)
			index++;
		
		bool pressed = (s_keyboard_key_buffer[index] & 0x80) == 0;
		uint8_t ch = s_keyboard_key_buffer[index] & ~0x80;

		Key key = Key::INVALID;

		if (extended)
		{
			key = scan_code_to_key_extended[ch];
		}
		else
		{
			if (s_modifiers & MOD_SHIFT)
				key = scan_code_to_key_shift[ch];
			else if (s_modifiers & MOD_ALTGR)
				key = scan_code_to_key_altgr[ch];
			else
				key = scan_code_to_key_normal[ch];
		}

		if ((s_led_states & I8042_KB_LED_NUM_LOCK))
		{
			switch (key)
			{
				case Key::Numpad0:		key = Key::Insert;		break;
				case Key::Numpad1:		key = Key::End;			break;
				case Key::Numpad2:		key = Key::Down;		break;
				case Key::Numpad3:		key = Key::PageDown;	break;
				case Key::Numpad4:		key = Key::Left;		break;
				case Key::Numpad5:		key = Key::None;		break;
				case Key::Numpad6:		key = Key::Right;		break;
				case Key::Numpad7:		key = Key::Home;		break;
				case Key::Numpad8:		key = Key::Up;			break;
				case Key::Numpad9:		key = Key::PageUp;		break;
				case Key::NumpadSep:	key = Key::Delete;		break;
				default: break;
			}
		}


#if KEYBOARD_SHOW_UNKNOWN
		if (key == Key::INVALID)
			kprintln("{} {}", ch, extended ? 'E' : ' ');
#endif

		s_keyboard_state[(int)key] = pressed;

		bool update_leds = false;
		switch (key)
		{
			case Key::ScrollLock:
				update_leds = pressed;
				if (update_leds)
					s_led_states ^= I8042_KB_LED_SCROLL_LOCK;
				break;
			case Key::NumLock:
				update_leds = pressed;
				if (update_leds)
					s_led_states ^= I8042_KB_LED_NUM_LOCK;
				break;
			case Key::CapsLock:
				update_leds = pressed;
				if (update_leds)
					s_led_states ^= I8042_KB_LED_CAPS_LOCK;
				break;
			default:
				break;
		}

		if (update_leds)
		{
			s_command_queue.Push({
				.target = TARGET_KEYBOARD,
				.command = I8042_KB_SET_LEDS,
				.data = s_led_states,
				.has_data = true,
			});
		}

		uint8_t modifiers = 0;
		if (s_keyboard_state[(int)Key::LeftShift] || s_keyboard_state[(int)Key::RightShift])
			modifiers |= MOD_SHIFT;
		if (s_keyboard_state[(int)Key::LeftCtrl] || s_keyboard_state[(int)Key::RightCtrl])
			modifiers |= MOD_CTRL;
		if (s_keyboard_state[(int)Key::LeftAlt])
			modifiers |= MOD_ALT;
		if (s_keyboard_state[(int)Key::RightAlt])
			modifiers |= MOD_ALTGR;
		if (s_led_states & I8042_KB_LED_CAPS_LOCK)
			modifiers |= MOD_CAPS;
		s_modifiers = modifiers;

		if (key != Key::INVALID)
		{
			auto error_or = s_key_event_queue.Push({ .key = key, .modifiers = modifiers, .pressed = pressed });
			if (error_or.IsError())
				dwarnln("{}", error_or.GetError());
		}
		s_keyboard_key_buffer_size -= index + 1;
		memmove(s_keyboard_key_buffer, s_keyboard_key_buffer + index, s_keyboard_key_buffer_size);
	}

	static void keyboard_irq_handler()
	{
		uint8_t raw = IO::inb(I8042_DATA_PORT);
#if DEBUG_ALL_IRQ
		dprintln("k 0x{2H}", raw);
#endif
		i8042_handle_byte(TARGET_KEYBOARD, raw);
	}

	static void mouse_irq_handler()
	{
		uint8_t raw = IO::inb(I8042_DATA_PORT);
#if DEBUG_ALL_IRQ
		dprintln("m 0x{2H}", raw);
#endif
		i8042_handle_byte(TARGET_MOUSE, raw);
	}

	static void initialize_keyboard()
	{
		// Register callback and IRQ
		IDT::register_irq_handler(KEYBOARD_IRQ, keyboard_irq_handler);
		APIC::EnableIRQ(KEYBOARD_IRQ);
		i8042_controller_command(I8042_ENABLE_FIRST_PORT);

		MUST(s_command_queue.Push({
			.target = TARGET_KEYBOARD,
			.command = I8042_KB_RESET,
			.resp_cnt = 1,
		}));

		// Set scan code set 2
		MUST(s_command_queue.Push({
			.target = TARGET_KEYBOARD,
			.command = I8042_KB_SET_SCAN_CODE_SET,
			.data = 0x02,
			.has_data = true,
		}));

		// Turn LEDs off
		MUST(s_command_queue.Push({
			.target = TARGET_KEYBOARD,
			.command = I8042_KB_SET_LEDS,
			.data = s_led_states,
			.has_data = true,
		}));
	}

	static void initialize_mouse()
	{
		// Register callback and IRQ
		IDT::register_irq_handler(MOUSE_IRQ, mouse_irq_handler);
		APIC::EnableIRQ(MOUSE_IRQ);
		i8042_controller_command(I8042_ENABLE_SECOND_PORT);

		MUST(s_command_queue.Push({
			.target = TARGET_MOUSE,
			.command = I8042_MOUSE_RESET,
			.resp_cnt = 2,
		}));

		// Mouse should be disabled when sending commands
		MUST(s_command_queue.Push({
			.target = TARGET_MOUSE,
			.command = I8042_MOUSE_ENABLE,
		}));
	}

	bool initialize()
	{
		// https://wiki.osdev.org/%228042%22_PS/2_Controller

		// Step 1: Initialize USB Controllers
		// TODO

		// Stem 2: Determine if the PS/2 Controller Exists
		// TODO

		// Step 3: Disable Devices
		i8042_controller_command(I8042_DISABLE_FIRST_PORT);
		i8042_controller_command(I8042_DISABLE_SECOND_PORT);

		// Step 4: Flush The Ouput Buffer
		while ((IO::inb(I8042_STATUS_REGISTER) & I8042_STATUS_OUT_FULL) != 0)
			IO::inb(I8042_DATA_PORT);

		// Step 5: Set the Controller Configuration Byte
		i8042_controller_command(I8042_READ_CONFIG);
		uint8_t config = wait_and_read();
		config &= ~(I8042_CONFING_IRQ_FIRST | I8042_CONFING_IRQ_SECOND);
		i8042_controller_command(I8042_WRITE_CONFIG, config);


		// Step 6: Perform Controller Self Test
		i8042_controller_command(I8042_TEST_CONTROLLER);
		if (wait_and_read() != I8042_TEST_CONTROLLER_PASS)
		{
			derrorln("PS/2 controller self test failed");
			return false;
		}

		// Step 7: Determine If There Are 2 Channels
		bool dual_channel = MOUSE_ENABLED ? config & I8042_CONFING_DUAL_CHANNEL : false;
		if (dual_channel)
		{
			i8042_controller_command(I8042_ENABLE_SECOND_PORT);
			i8042_controller_command(I8042_READ_CONFIG);
			if (wait_and_read() & I8042_CONFING_DUAL_CHANNEL)
				dual_channel = false;
			else
				i8042_controller_command(I8042_DISABLE_SECOND_PORT);
		}

		// Step 8: Perform Interface Tests
		i8042_controller_command(I8042_TEST_FIRST_PORT);
		if (wait_and_read() != I8042_TEST_FIRST_PORT_PASS)
		{
			derrorln("PS/2 first port test failed");
			return false;
		}

		if (dual_channel)
		{
			i8042_controller_command(I8042_TEST_SECOND_PORT);
			if (wait_and_read() != I8042_TEST_SECOND_PORT_PASS)
			{
				dwarnln("PS/2 second port test failed. Mouse will be disabled");
				dual_channel = false;
			}
		}

		// Step 9: Enable Devices
		config |= I8042_CONFING_IRQ_FIRST;
		if (dual_channel)
			config |= I8042_CONFING_IRQ_SECOND;
		i8042_controller_command(I8042_WRITE_CONFIG, config);

		// Step 10: Reset Devices
		initialize_keyboard();
		if (dual_channel)
			initialize_mouse();

		return true;
	}

	void register_key_event_callback(void (*callback)(KeyEvent))
	{
		s_key_event_callback = callback;
	}

	void register_mouse_button_event_callback(void (*callback)(MouseButtonEvent))
	{
		s_mouse_button_event_callback = callback;
	}

	void register_mouse_move_event_callback(void (*callback)(MouseMoveEvent))
	{
		s_mouse_move_event_callback = callback;
	}

	const char* key_event_to_utf8(KeyEvent event)
	{
		bool shift = event.modifiers & MOD_SHIFT;
		bool caps = event.modifiers & MOD_CAPS;
		if (shift ^ caps)
			return s_key_to_utf8_upper[(int)event.key];
		return s_key_to_utf8_lower[(int)event.key];
	}

}