#include <kernel/USB/HID/Keyboard.h>
#include <LibInput/KeyEvent.h>

#define DEBUG_KEYBOARD 0

namespace Kernel
{

	static BAN::Optional<uint8_t> s_scancode_to_keycode[0x100] {};
	static bool s_scancode_to_keycode_initialized = false;

	static void initialize_scancode_to_keycode();

	void USBKeyboard::start_report()
	{
		for (auto& val : m_keyboard_state_temp)
			val = false;
	}

	void USBKeyboard::stop_report()
	{
		using KeyModifier = LibInput::KeyEvent::Modifier;

		if (!s_scancode_to_keycode_initialized)
			initialize_scancode_to_keycode();

		// FIXME: RawKeyEvent probably should only contain keycode.
		//        Modifier should be determined when converting to KeyEvent.

		uint8_t modifier = 0;
		if (m_keyboard_state_temp[0xE1])
			modifier |= KeyModifier::LShift;
		if (m_keyboard_state_temp[0xE5])
			modifier |= KeyModifier::RShift;
		if (m_keyboard_state_temp[0xE0])
			modifier |= KeyModifier::LCtrl;
		if (m_keyboard_state_temp[0xE4])
			modifier |= KeyModifier::RCtrl;
		if (m_keyboard_state_temp[0xE2])
			modifier |= KeyModifier::LAlt;
		if (m_keyboard_state_temp[0xE6])
			modifier |= KeyModifier::RAlt;

		if (m_keyboard_state_temp[0x39] && !m_keyboard_state[0x39])
			m_toggle_mask ^= KeyModifier::CapsLock;
		if (m_keyboard_state_temp[0x47] && !m_keyboard_state[0x47])
			m_toggle_mask ^= KeyModifier::ScrollLock;
		if (m_keyboard_state_temp[0x53] && !m_keyboard_state[0x53])
			m_toggle_mask ^= KeyModifier::NumLock;

		modifier |= m_toggle_mask;

		for (size_t i = 0; i < m_keyboard_state.size(); i++)
		{
			if (m_keyboard_state[i] == m_keyboard_state_temp[i])
				continue;

			const bool pressed = m_keyboard_state_temp[i];

			if (pressed)
				dprintln_if(DEBUG_KEYBOARD, "{2H}", i);

			auto opt_keycode = s_scancode_to_keycode[i];
			if (opt_keycode.has_value())
			{
				LibInput::RawKeyEvent event;
				event.keycode = opt_keycode.value();
				event.modifier = modifier | (pressed ? KeyModifier::Pressed : 0);
				add_event(BAN::ConstByteSpan::from(event));
			}

			m_keyboard_state[i] = m_keyboard_state_temp[i];
		}
	}

	void USBKeyboard::handle_variable(uint16_t usage_page, uint16_t usage, int64_t state)
	{
		if (usage_page != 0x07)
		{
			dprintln_if(DEBUG_KEYBOARD, "Unsupported keyboard usage page {2H}", usage_page);
			return;
		}
		if (usage >= 4 && usage < m_keyboard_state_temp.size())
			m_keyboard_state_temp[usage] = state;
	}

	void USBKeyboard::handle_array(uint16_t usage_page, uint16_t usage)
	{
		if (usage_page != 0x07)
		{
			dprintln_if(DEBUG_KEYBOARD, "Unsupported keyboard usage page {2H}", usage_page);
			return;
		}
		if (usage >= 4 && usage < m_keyboard_state_temp.size())
			m_keyboard_state_temp[usage] = true;
	}

	void initialize_scancode_to_keycode()
	{
		using LibInput::keycode_function;
		using LibInput::keycode_normal;
		using LibInput::keycode_numpad;

		s_scancode_to_keycode[0x35] = keycode_normal(0,  0);
		s_scancode_to_keycode[0x1E] = keycode_normal(0,  1);
		s_scancode_to_keycode[0x1F] = keycode_normal(0,  2);
		s_scancode_to_keycode[0x20] = keycode_normal(0,  3);
		s_scancode_to_keycode[0x21] = keycode_normal(0,  4);
		s_scancode_to_keycode[0x22] = keycode_normal(0,  5);
		s_scancode_to_keycode[0x23] = keycode_normal(0,  6);
		s_scancode_to_keycode[0x24] = keycode_normal(0,  7);
		s_scancode_to_keycode[0x25] = keycode_normal(0,  8);
		s_scancode_to_keycode[0x26] = keycode_normal(0,  9);
		s_scancode_to_keycode[0x27] = keycode_normal(0, 10);
		s_scancode_to_keycode[0x2D] = keycode_normal(0, 11);
		s_scancode_to_keycode[0x2E] = keycode_normal(0, 12);
		s_scancode_to_keycode[0x2A] = keycode_normal(0, 13);
		s_scancode_to_keycode[0x2B] = keycode_normal(1,  0);
		s_scancode_to_keycode[0x14] = keycode_normal(1,  1);
		s_scancode_to_keycode[0x1A] = keycode_normal(1,  2);
		s_scancode_to_keycode[0x08] = keycode_normal(1,  3);
		s_scancode_to_keycode[0x15] = keycode_normal(1,  4);
		s_scancode_to_keycode[0x17] = keycode_normal(1,  5);
		s_scancode_to_keycode[0x1C] = keycode_normal(1,  6);
		s_scancode_to_keycode[0x18] = keycode_normal(1,  7);
		s_scancode_to_keycode[0x0C] = keycode_normal(1,  8);
		s_scancode_to_keycode[0x12] = keycode_normal(1,  9);
		s_scancode_to_keycode[0x13] = keycode_normal(1, 10);
		s_scancode_to_keycode[0x2F] = keycode_normal(1, 11);
		s_scancode_to_keycode[0x30] = keycode_normal(1, 12);
		s_scancode_to_keycode[0x39] = keycode_normal(2,  0);
		s_scancode_to_keycode[0x04] = keycode_normal(2,  1);
		s_scancode_to_keycode[0x16] = keycode_normal(2,  2);
		s_scancode_to_keycode[0x07] = keycode_normal(2,  3);
		s_scancode_to_keycode[0x09] = keycode_normal(2,  4);
		s_scancode_to_keycode[0x0A] = keycode_normal(2,  5);
		s_scancode_to_keycode[0x0B] = keycode_normal(2,  6);
		s_scancode_to_keycode[0x0D] = keycode_normal(2,  7);
		s_scancode_to_keycode[0x0E] = keycode_normal(2,  8);
		s_scancode_to_keycode[0x0F] = keycode_normal(2,  9);
		s_scancode_to_keycode[0x33] = keycode_normal(2, 10);
		s_scancode_to_keycode[0x34] = keycode_normal(2, 11);
		s_scancode_to_keycode[0x31] = keycode_normal(2, 12);
		s_scancode_to_keycode[0x28] = keycode_normal(2, 13);
		s_scancode_to_keycode[0xE1] = keycode_normal(3,  0);
		s_scancode_to_keycode[0x64] = keycode_normal(3,  1);
		s_scancode_to_keycode[0x1D] = keycode_normal(3,  2);
		s_scancode_to_keycode[0x1B] = keycode_normal(3,  3);
		s_scancode_to_keycode[0x06] = keycode_normal(3,  4);
		s_scancode_to_keycode[0x19] = keycode_normal(3,  5);
		s_scancode_to_keycode[0x05] = keycode_normal(3,  6);
		s_scancode_to_keycode[0x11] = keycode_normal(3,  7);
		s_scancode_to_keycode[0x10] = keycode_normal(3,  8);
		s_scancode_to_keycode[0x36] = keycode_normal(3,  9);
		s_scancode_to_keycode[0x37] = keycode_normal(3, 10);
		s_scancode_to_keycode[0x38] = keycode_normal(3, 11);
		s_scancode_to_keycode[0xE5] = keycode_normal(3, 12);
		s_scancode_to_keycode[0xE0] = keycode_normal(4,  1);
		s_scancode_to_keycode[0xE3] = keycode_normal(4,  2);
		s_scancode_to_keycode[0xE2] = keycode_normal(4,  3);
		s_scancode_to_keycode[0x2C] = keycode_normal(4,  4);
		s_scancode_to_keycode[0xE6] = keycode_normal(4,  5);
		s_scancode_to_keycode[0xE4] = keycode_normal(4,  6);

		s_scancode_to_keycode[0x52] = keycode_normal(5,  0);
		s_scancode_to_keycode[0x50] = keycode_normal(5,  1);
		s_scancode_to_keycode[0x51] = keycode_normal(5,  2);
		s_scancode_to_keycode[0x4F] = keycode_normal(5,  3);

		s_scancode_to_keycode[0x29] = keycode_function( 0);
		s_scancode_to_keycode[0x3A] = keycode_function( 1);
		s_scancode_to_keycode[0x3B] = keycode_function( 2);
		s_scancode_to_keycode[0x3C] = keycode_function( 3);
		s_scancode_to_keycode[0x3D] = keycode_function( 4);
		s_scancode_to_keycode[0x3E] = keycode_function( 5);
		s_scancode_to_keycode[0x3F] = keycode_function( 6);
		s_scancode_to_keycode[0x40] = keycode_function( 7);
		s_scancode_to_keycode[0x41] = keycode_function( 8);
		s_scancode_to_keycode[0x42] = keycode_function( 9);
		s_scancode_to_keycode[0x43] = keycode_function(10);
		s_scancode_to_keycode[0x44] = keycode_function(11);
		s_scancode_to_keycode[0x45] = keycode_function(12);
		s_scancode_to_keycode[0x49] = keycode_function(13);
		s_scancode_to_keycode[0x46] = keycode_function(14);
		s_scancode_to_keycode[0x4C] = keycode_function(15);
		s_scancode_to_keycode[0x4A] = keycode_function(16);
		s_scancode_to_keycode[0x4D] = keycode_function(17);
		s_scancode_to_keycode[0x4B] = keycode_function(18);
		s_scancode_to_keycode[0x4E] = keycode_function(19);

		s_scancode_to_keycode[0x53] = keycode_numpad(0, 0);
		s_scancode_to_keycode[0x54] = keycode_numpad(0, 1);
		s_scancode_to_keycode[0x55] = keycode_numpad(0, 2);
		s_scancode_to_keycode[0x56] = keycode_numpad(0, 3);
		s_scancode_to_keycode[0x5F] = keycode_numpad(1, 0);
		s_scancode_to_keycode[0x60] = keycode_numpad(1, 1);
		s_scancode_to_keycode[0x61] = keycode_numpad(1, 2);
		s_scancode_to_keycode[0x57] = keycode_numpad(1, 3);
		s_scancode_to_keycode[0x5C] = keycode_numpad(2, 0);
		s_scancode_to_keycode[0x5D] = keycode_numpad(2, 1);
		s_scancode_to_keycode[0x5E] = keycode_numpad(2, 2);
		s_scancode_to_keycode[0x59] = keycode_numpad(3, 0);
		s_scancode_to_keycode[0x5A] = keycode_numpad(3, 1);
		s_scancode_to_keycode[0x5B] = keycode_numpad(3, 2);
		s_scancode_to_keycode[0x58] = keycode_numpad(3, 3);
		s_scancode_to_keycode[0x62] = keycode_numpad(4, 0);
		s_scancode_to_keycode[0x63] = keycode_numpad(4, 1);

		s_scancode_to_keycode_initialized = true;
	}

}
