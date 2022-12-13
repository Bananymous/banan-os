#include <BAN/Queue.h>
#include <kernel/IDT.h>
#include <kernel/IO.h>
#include <kernel/Keyboard.h>
#include <kernel/kprint.h>
#include <kernel/PIC.h>
#include <kernel/PIT.h>
#include <kernel/Serial.h>

#include <kernel/KeyboardLayout/FI.h>

#define KB_DEBUG_PRINT 1

#define I8042_DATA_PORT				0x60
#define I8042_STATUS_REGISTER		0x64
#define I8042_COMMAND_REGISTER		0x64

#define I8042_STATUS_OUT_FULL		(1 << 0)
#define I8042_STATUS_IN_FULL		(1 << 1)

#define I8042_READ_CONFIG			0x20
#define I8042_WRITE_CONFIG			0x60

#define I8042_CONFING_IRQ_FIRST		(1 << 0)
#define I8042_CONFING_IRQ_SECOND	(1 << 1)
#define I8042_CONFING_TRANSLATION	(1 << 6)

#define I8042_ENABLE_FIRST			0xAE
#define I8042_ENABLE_SECOND			0xA8
#define I8042_DISABLE_FIRST			0xAD
#define I8042_DISABLE_SECOND		0xA7

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
#define I8042_KB_TIMEOUT_MS			1000
#define I8042_KB_LED_SCROLL_LOCK	(1 << 0)
#define I8042_KB_LED_NUM_LOCK		(1 << 1)
#define I8042_KB_LED_CAPS_LOCK		(1 << 2)

#define KEYBOARD_IRQ				0x01

#define MOD_ALT		(1 << 0)
#define MOD_CTRL	(1 << 1)
#define MOD_SHIFT	(1 << 2)
#define MOD_ALTGR	(1 << 3)
#define MOD_CAPS	(1 << 4)

namespace Keyboard
{

	static bool s_keyboard_state[0xFF] = {};

	struct Command
	{
		uint8_t	command		= 0;
		uint8_t	data		= 0;
		bool	has_data	= false;
		bool	extra		= false;
		uint8_t	_sent		= 0;
		uint8_t _ack		= 0;
		bool	_done		= false;
	};
	static BAN::Queue<Command>	s_keyboard_command_queue;
	static uint8_t				s_keyboard_command_extra = 0x00;

	static BAN::Queue<KeyEvent> s_key_event_queue;
	static uint8_t s_keyboard_key_buffer[10] = {};
	static uint8_t s_keyboard_key_buffer_size = 0;

	static uint8_t s_led_states	= 0b000;
	static uint8_t s_modifiers	= 0x00;

	static void (*s_key_event_callback)(KeyEvent) = nullptr;

	static char s_key_to_char[]
	{
		'\0', '\0',
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
		'A', 'A', 'O',

		',', ':', '.', ';', '-', '_', '\'', '*', '^', '~',
		'!', '?', '"', '#', '%', '&', '/', '\\', '+', '=',
		'(', ')', '[', ']', '{', '}',
		'$', '\0', '\0', '\0', '\n', ' ', '\t', '\b', '<', '>', '\0', '`', '\0', '\0', '@', '|',
		'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',

		'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',

		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		',', '+', '*', '/', '-', '\n',

		'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
 

		'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
	};
	static_assert(sizeof(s_key_to_char) == static_cast<int>(Key::Count));

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

	static bool i8042_keyboard_command(uint8_t command)
	{
		auto timeout = PIT::ms_since_boot() + I8042_KB_TIMEOUT_MS;

		while (PIT::ms_since_boot() < timeout)
		{
			if ((IO::inb(I8042_STATUS_REGISTER) & I8042_STATUS_IN_FULL) == 0)
			{
				IO::outb(I8042_DATA_PORT, command);
				return true;
			}
		}

		return false;
	}

	void update_keyboard()
	{
		if (!s_keyboard_command_queue.Empty())
		{
			auto& command = s_keyboard_command_queue.Front();

			if (command._sent == 0 && command._ack == 0)
			{
				if (!i8042_keyboard_command(command.command))
					Kernel::panic("oof 1");
				command._sent++;
			}

			if (command._sent == 1 && command._ack == 1 && command.has_data)
			{
				if (!i8042_keyboard_command(command.data))
					Kernel::panic("oof 2");
				command._sent++;
			}
			
			if (command._done)
			{
				switch (command.command)
				{
					case I8042_KB_RESET:
						if (s_keyboard_command_extra != I8042_KB_SELF_TEST_PASS)
							Kernel::panic("PS/2 Keyboard self test failed");
						break;
					case I8042_KB_SET_SCAN_CODE_SET:
						break;
					case I8042_KB_SET_LEDS:
						break;
				}
				s_keyboard_command_queue.Pop();
			}
		}

		while (!s_key_event_queue.Empty())
		{
			if (s_key_event_callback)
				s_key_event_callback(s_key_event_queue.Front());
			s_key_event_queue.Pop();
		}
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


#if KB_DEBUG_PRINT
		if (key == Key::INVALID)
			kprintln("{} {}", ch, extended ? 'E' : ' ');
#endif

		s_keyboard_state[static_cast<int>(key)] = pressed;

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
			s_keyboard_command_queue.Push({
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

	void keyboard_irq_handler()
	{
		uint8_t raw = IO::inb(I8042_DATA_PORT);

		bool command_waiting = false;
		if (!s_keyboard_command_queue.Empty())
		{
			auto& command = s_keyboard_command_queue.Front();
			command_waiting = (command._sent > 0 && !command._done);
		}

		if (command_waiting)
		{
			auto& command = s_keyboard_command_queue.Front();
			if (raw == I8042_KB_RESEND)
			{
				dprintln("PS/2 Keyboard: Resend 0x{H}", command._sent == 2 ? command.data : command.command);
				command._sent--;
				return;
			}

			if (raw == I8042_KB_ACK)
			{
				command._ack++;
				if (command.extra > 0)
					return;
				command._done = command.has_data ? (command._ack == 2) : true;
				return;
			}

			if (raw == 0x00)
			{
				dprintln("\e[33mKey detection error or internal buffer overrun\e[m");
				command._sent = 0;
				command._ack = 0;
				command._done = false;
				return;
			}

			if (raw == 0xEE && command.command == 0xEE)
			{
				s_keyboard_command_queue.Pop();
				return;
			}

			s_keyboard_command_extra = raw;
			command._done = true;
			return;
		}
		else
		{
			s_keyboard_key_buffer[s_keyboard_key_buffer_size++] = raw;
			if (raw != 0xE0)
				keyboard_new_key();
		}
	}

	bool initialize()
	{
		// https://wiki.osdev.org/%228042%22_PS/2_Controller

		// TODO: support dual channel

		// Step 1: Initialize USB Controllers
		// TODO

		// Stem 2: Determine if the PS/2 Controller Exists
		// TODO

		// Step 3: Disable Devices
		i8042_controller_command(I8042_DISABLE_FIRST);
		i8042_controller_command(I8042_DISABLE_SECOND);

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

		// Step 8: Perform Interface Tests
		i8042_controller_command(I8042_TEST_FIRST_PORT);
		if (wait_and_read() != I8042_TEST_FIRST_PORT_PASS)
		{
			derrorln("PS/2 first port test failed");
			return false;
		}

		// Step 9: Enable Devices
		config |= I8042_CONFING_IRQ_FIRST;
		i8042_controller_command(I8042_WRITE_CONFIG, config);
		i8042_controller_command(I8042_ENABLE_FIRST);

		// Step 10: Reset Devices
		MUST(s_keyboard_command_queue.Push({
			.command = I8042_KB_RESET,
			.extra = true,
		}));

		// Set scan code set 2
		MUST(s_keyboard_command_queue.Push({
			.command = I8042_KB_SET_SCAN_CODE_SET,
			.data = 0x02,
			.has_data = true,
		}));

		// Turn LEDs off
		MUST(s_keyboard_command_queue.Push({
			.command = I8042_KB_SET_LEDS,
			.data = s_led_states,
			.has_data = true,
		}));

		// Register callback and IRQ
		IDT::register_irq_handler(KEYBOARD_IRQ, keyboard_irq_handler);
		PIC::unmask(KEYBOARD_IRQ);

		return true;
	}

	void register_key_event_callback(void(*callback)(KeyEvent))
	{
		s_key_event_callback = callback;
	}

	char key_event_to_ascii(KeyEvent event)
	{
		char res = s_key_to_char[static_cast<uint8_t>(event.key)];

		if (!(event.modifiers & (MOD_SHIFT | MOD_CAPS)))
			if (res >= 'A' && res <= 'Z')
				res = res - 'A' + 'a';

		return res;
	}

	void led_disco()
	{
		uint64_t	time	= PIT::ms_since_boot();
		uint64_t	freq	= 100;
		bool		state	= false;
		for(;;)
		{
			if (PIT::ms_since_boot() > time + freq)
			{
				time += freq;
				state = !state;

				MUST(s_keyboard_command_queue.Push({
					.command = I8042_KB_SET_LEDS,
					.data = (uint8_t)(state ? 0b111 : 0b000),
					.has_data = true,
				}));
			}

			update_keyboard();
		}
	}

}