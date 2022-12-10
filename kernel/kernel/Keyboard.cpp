#include <kernel/IDT.h>
#include <kernel/IO.h>
#include <kernel/Keyboard.h>
#include <kernel/PIC.h>
#include <kernel/kprint.h>

#include <kernel/KeyboardLayout/FI.h>

#define I8042_DATA_PORT 0x60
#define I8042_STATUS_REGISTER 0x64
#define I8042_COMMAND_REGISTER 0x64

#define I8042_READ_BYTE0 0x20
#define I8042_WRITE_BYTE0 0x60

#define I8042_ENABLE_FIRST 0xAE
#define I8042_ENABLE_SECOND 0xA8
#define I8042_DISABLE_FIRST 0xAD
#define I8042_DISABLE_SECOND 0xA7

#define I8042_TEST_CONTROLLER 0xAA
#define I8042_CONTROLLER_TEST_PASS 0x55

#define I8042_TEST_FIRST_PORT 0xAB
#define I8042_FIRST_PORT_TEST_PASS 0x00

#define I8042_TEST_SECOND 0xA9
#define I8042_SECOND_PORT_TEST_PASS 0x00

#define I8042_ACK 0xfa

#define KEYBOARD_IRQ 0x01

#define MOD_ALT		0b0001
#define MOD_CTRL	0b0010
#define MOD_SHIFT	0b0100
#define MOD_ALTGR	0b1000

#define KEYBOARD_FI

namespace Keyboard
{

	static bool		s_keyboard_state[0xFF]	= {};
	static uint8_t	s_modifiers				= 0;

	static void (*s_key_callback)(Key, uint8_t, bool) = nullptr;

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

		'\0', '\0', '\0', '\0', '\0', '\0', '\0',

		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		',', '+', '*', '/', '-',

		'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
	};
	static_assert(sizeof(s_key_to_char) == static_cast<int>(Key::Count));

	static uint8_t kb_read()
	{
		while (!(IO::inb(I8042_STATUS_REGISTER) & 0x01))
			continue;
		return IO::inb(I8042_DATA_PORT);
	}

	static bool kb_try_read(uint8_t& out)
	{
		if (IO::inb(I8042_STATUS_REGISTER) & 0x01)
		{
			out = IO::inb(I8042_DATA_PORT);
			return true;
		}
		return false;
	}

	static void kb_command(uint8_t command)
	{
		IO::io_wait();
		IO::outb(I8042_COMMAND_REGISTER, command);
	}

	static void kb_command(uint8_t command, uint8_t data)
	{
		IO::io_wait();
		IO::outb(I8042_COMMAND_REGISTER, command);
		IO::io_wait();
		IO::outb(I8042_DATA_PORT, data);
	}

	void irq_handler()
	{
		uint8_t ch;
		while (kb_try_read(ch))
		{
			bool multimedia = false, pressed = true;

			if (ch == 0xE0)
			{
				multimedia = true;
				ch = kb_read();
			}

			if (ch == 0xF0)
			{
				pressed = false;
				ch = kb_read();
			}

			// TODO: Handle multimedia keys
			if (multimedia)
			{
				if (ch == 17)
					pressed ? (s_modifiers |= MOD_ALTGR) : (s_modifiers &= ~MOD_ALTGR);
				if (pressed && false)
					kprint("<M{}>", ch);
				continue;
			}
			
			s_keyboard_state[ch] = pressed;

			switch (ch)
			{
				case 17:
					pressed ? (s_modifiers |= MOD_ALT) : (s_modifiers &= ~MOD_ALT);
					break;
				case 18:
				case 89:
					pressed ? (s_modifiers |= MOD_SHIFT) : (s_modifiers &= ~MOD_SHIFT);
					break;
				case 20:
					pressed ? (s_modifiers |= MOD_CTRL) : (s_modifiers &= ~MOD_CTRL);
					break;
				default:
					break;
			}

			Key key;
			if (s_modifiers & MOD_ALTGR)
				key = scs2_to_key_altgr[ch];
			else if (s_modifiers & MOD_SHIFT)
				key = scs2_to_key_shift[ch];
			else
				key = scs2_to_key[ch];
			
			// Debug print for unregistered keys
			if (key == Key::INVALID && pressed)
				kprint("<{}>", ch);
			
			s_key_callback(key, s_modifiers, pressed);
		}
	}

	void initialize(void (*callback)(Key, uint8_t, bool))
	{
		// https://wiki.osdev.org/%228042%22_PS/2_Controller

		// TODO: support dual channel

		// Step 1: Initialize USB Controllers
		// TODO

		// Stem 2: Determine if the PS/2 Controller Exists
		// TODO

		// Step 3: Disable Devices
		kb_command(I8042_DISABLE_FIRST);
		kb_command(I8042_DISABLE_SECOND);

		// Step 4: Flush The Ouput Buffer
		uint8_t tmp;
		while(kb_try_read(tmp))
			continue;

		// Step 5: Set the Controller Configuration Byte
		kb_command(I8042_READ_BYTE0);
		uint8_t conf = kb_read();
		conf &= 0b10111100;
		kb_command(I8042_WRITE_BYTE0, conf);

		// Step 6: Perform Controller Self Test
		kb_command(I8042_TEST_CONTROLLER);
		uint8_t resp = kb_read();
		if (resp != I8042_CONTROLLER_TEST_PASS)
		{
			kprint("ERROR: PS/2 self test failed\n");
			return;
		}

		// Step 7: Determine If There Are 2 Channels

		// Step 8: Perform Interface Tests
		kb_command(I8042_TEST_FIRST_PORT);
		resp = kb_read();
		if (resp != I8042_FIRST_PORT_TEST_PASS)
		{
			kprint("ERROR: PS/2 interface test failed\n");
			return;
		}

		// Step 9: Enable Devices
		kb_command(I8042_WRITE_BYTE0, conf | 0x01); // enable IRQs
		kb_command(I8042_ENABLE_FIRST);

		// Step 10: Reset Devices
		/* TODO: doesnt seem to respond
		kb_command(0xFF);
		resp = kb_read();
		if (resp != PS2_ACK)
		{
			kprint("ERROR: PS/2 could not restart devices\n");
			return;
		}
		*/

		// Register callback and IRQ
		s_key_callback = callback;
		IDT::register_irq_handler(KEYBOARD_IRQ, irq_handler);
		PIC::unmask(KEYBOARD_IRQ);

		kb_command(0xED, 0b111);
		IO::io_wait();
		while (kb_try_read(tmp));
	}

	char key_to_ascii(Key key, uint8_t modifiers)
	{
		char res = s_key_to_char[static_cast<uint8_t>(key)];

		if (!(modifiers & MOD_SHIFT))
			if (res >= 'A' && res <= 'Z')
				res = res - 'A' + 'a';

		return res;
	}

}