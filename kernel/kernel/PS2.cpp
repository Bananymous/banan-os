#include <kernel/IDT.h>
#include <kernel/IO.h>
#include <kernel/PIC.h>
#include <kernel/PS2.h>
#include <kernel/kprint.h>

#include <string.h>

#define PS2_DATA_PORT 0x60
#define PS2_STATUS_REGISTER 0x64
#define PS2_COMMAND_REGISTER 0x64

#define PS2_ACK 0xfa

#define PS2_IRQ 0x01

#define ALT		0
#define CTRL	1
#define SHIFT	2

#define KEYBOARD_FI

namespace PS2
{

	static bool		s_keyboard_state[0x7f];
	static uint8_t	s_modifiers = 0;

#ifdef KEYBOARD_FI
	enum class Key
	{
		Key_INVALID,
		Key_ESC,
		Key_1,
		Key_2,
		Key_3,
		Key_4,
		Key_5,
		Key_6,
		Key_7,
		Key_8,
		Key_9,
		Key_0,
		Key_Plus,
		Key_Tick,
		Key_Backspace,
		Key_Tab,
		Key_Q,
		Key_W,
		Key_E,
		Key_R,
		Key_T,
		Key_Y,
		Key_U,
		Key_I,
		Key_O,
		Key_P,
		Key_Å,
		Key_Caret,
		Key_Enter,
		Key_Control,
		Key_A,
		Key_S,
		Key_D,
		Key_F,
		Key_G,
		Key_H,
		Key_J,
		Key_K,
		Key_L,
		Key_Ö,
		Key_Ä,
		Key_Section, // §
		Key_LeftShift,
		Key_SingleQuote,
		Key_Z,
		Key_X,
		Key_C,
		Key_V,
		Key_B,
		Key_N,
		Key_M,
		Key_Comma,
		Key_Period,
		Key_Hyphen,
		Key_RightShift,
		Key_Unknown,	// Maybe other alt
		Key_Alt,
		Key_Space,
		Key_CapsLock,
		Key_F1,
		Key_F2,
		Key_F3,
		Key_F4,
		Key_F5,
		Key_F6,
		Key_F7,
		Key_F8,
		Key_F9,
		Key_F10,
		// TODO
	};
#endif






























































































	char fi_keymap[0x7f]
	{
		'\0',
		'\0', // esc
		'1',
		'2',
		'3',
		'4',
		'5',
		'6',
		'7',
		'8',
		'9',
		'0',
		'+',
		'`',
		'\b',
		'\t', // tab
		'q',
		'w',
		'e',
		'r',
		't',
		'y',
		'u',
		'i',
		'o',
		'p',
		'?',
		'^',
		'\n', // enter
		'\0',
		'a',
		's',
		'd',
		'f',
		'g',
		'h',
		'j',
		'k',
		'l',
		'\0', // ö
		'\0', // ä
		'\0', // §
		'\0',
		'\'',
		'z', 
		'x',
		'c',
		'v',
		'b',
		'n',
		'm',
		',',
		'.',
		'-',
		'\0',
		'\0',
		'\0',
		' ',
	};

	char fi_keymap_shift[0x7f]
	{
		'\0',
		'\0', // esc
		'!',
		'"',
		'#',
		'\0', // ¤
		'%',
		'&',
		'/',
		'(',
		')',
		'=',
		'?',
		'`',
		'\b',
		'\t', // tab
		'Q',
		'W',
		'E',
		'R',
		'T',
		'Y',
		'U',
		'I',
		'O',
		'P',
		'?',
		'^',
		'\n', // enter
		'\0',
		'A',
		'S',
		'D',
		'F',
		'G',
		'H',
		'J',
		'K',
		'L',
		'\0', // ö
		'\0', // ä
		'\0', // ½
		'\0',
		'*',
		'Z', 
		'X',
		'C',
		'V',
		'B',
		'N',
		'M',
		';',
		':',
		'_',
		'\0',
		'\0',
		'\0',
		' ',
	};

	void irq_handler()
	{
		while (IO::inb(PS2_STATUS_REGISTER) & 0x01)
		{
			uint8_t raw = IO::inb(PS2_DATA_PORT);
			uint8_t ch = raw & 0x7f;
			bool pressed = !(raw & 0x80);

			switch (ch)
			{
				case 0x38: pressed ? (s_modifiers |= 1 << ALT  ) : (s_modifiers &= ~(1 << ALT  )); break;
				case 0x1d: pressed ? (s_modifiers |= 1 << CTRL ) : (s_modifiers &= ~(1 << CTRL )); break;
				case 0x2a: pressed ? (s_modifiers |= 1 << SHIFT) : (s_modifiers &= ~(1 << SHIFT)); break;
			default:
				s_keyboard_state[ch] = pressed;	
			}

			if (pressed)
			{
				char c = s_modifiers & (1 << SHIFT) ? fi_keymap_shift[ch] : fi_keymap[ch];
				if (c) kprint("{}", c);
			}

			IO::io_wait();
		}
	}

	void initialize()
	{
		// Clear keyboard buffer
		while (IO::inb(PS2_STATUS_REGISTER) & 0x01)
			IO::inb(PS2_DATA_PORT);

		memset(s_keyboard_state, 0, sizeof(s_keyboard_state));

		IDT::register_irq_handler(PS2_IRQ, irq_handler);
		PIC::unmask(PS2_IRQ);
	}

}