#include <kernel/Timer/Timer.h>
#include <kernel/USB/HID/Keyboard.h>
#include <LibInput/KeyEvent.h>

namespace Kernel
{

	static constexpr uint64_t s_repeat_initial_ms = 500;
	static constexpr uint64_t s_repeat_interval_ms = 50;

	static BAN::Optional<uint8_t> s_scancode_to_keycode[0x100] {};
	static bool s_scancode_to_keycode_initialized = false;

	static void initialize_scancode_to_keycode();
	static constexpr bool is_repeatable_scancode(uint8_t scancode);

	void USBKeyboard::start_report()
	{
		m_lock_state = m_keyboard_lock.lock();
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

		uint16_t modifier = 0;

#define READ_MODIFIER(scancode, key_modifier) \
		if (m_keyboard_state_temp[scancode])  \
			modifier |= key_modifier;
		READ_MODIFIER(0xE1, KeyModifier::LShift);
		READ_MODIFIER(0xE5, KeyModifier::RShift);
		READ_MODIFIER(0xE0, KeyModifier::LCtrl);
		READ_MODIFIER(0xE4, KeyModifier::RCtrl);
		READ_MODIFIER(0xE2, KeyModifier::LAlt);
		READ_MODIFIER(0xE6, KeyModifier::RAlt);
#undef READ_MODIFIER

#define READ_TOGGLE(scancode, key_modifier)                                 \
		if (m_keyboard_state_temp[scancode] && !m_keyboard_state[scancode]) \
			m_toggle_mask ^= key_modifier;
		READ_TOGGLE(0x39, KeyModifier::CapsLock);
		READ_TOGGLE(0x47, KeyModifier::ScrollLock);
		READ_TOGGLE(0x53, KeyModifier::NumLock);
#undef READ_TOGGLE

		modifier |= m_toggle_mask;

		BAN::Optional<uint8_t> new_scancode;
		for (size_t i = 0; i < m_keyboard_state.size(); i++)
		{
			if (m_keyboard_state[i] == m_keyboard_state_temp[i])
				continue;
			if (m_keyboard_state_temp[i] && is_repeatable_scancode(i))
				new_scancode = i;

			const bool pressed = m_keyboard_state_temp[i];
			if (pressed)
				dprintln_if(DEBUG_USB_KEYBOARD, "Pressed {2H}", i);

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

		if (m_repeat_scancode.has_value() && !m_keyboard_state_temp[m_repeat_scancode.value()])
			m_repeat_scancode.clear();
		m_repeat_modifier = modifier;

		if (new_scancode.has_value())
		{
			if (!m_repeat_scancode.has_value() || m_repeat_scancode.value() != new_scancode.value())
			{
				m_repeat_scancode = new_scancode;
				m_next_repeat_event_ms = SystemTimer::get().ms_since_boot() + s_repeat_initial_ms;
			}
		}

		m_keyboard_lock.unlock(m_lock_state);
	}

	void USBKeyboard::handle_variable(uint16_t usage_page, uint16_t usage, int64_t state)
	{
		ASSERT(m_keyboard_lock.current_processor_has_lock());

		if (usage_page != 0x07)
		{
			dprintln_if(DEBUG_USB_KEYBOARD, "Unsupported keyboard usage page {2H}", usage_page);
			return;
		}
		if (!state)
			return;
		if (usage >= 4 && usage < m_keyboard_state_temp.size())
			m_keyboard_state_temp[usage] = state;
	}

	void USBKeyboard::handle_array(uint16_t usage_page, uint16_t usage)
	{
		ASSERT(m_keyboard_lock.current_processor_has_lock());

		if (usage_page != 0x07)
		{
			dprintln_if(DEBUG_USB_KEYBOARD, "Unsupported keyboard usage page {2H}", usage_page);
			return;
		}
		if (usage >= 4 && usage < m_keyboard_state_temp.size())
			m_keyboard_state_temp[usage] = true;
	}

	void USBKeyboard::update()
	{
		using KeyModifier = LibInput::KeyEvent::Modifier;

		SpinLockGuard _(m_keyboard_lock);

		if (!m_repeat_scancode.has_value() || SystemTimer::get().ms_since_boot() < m_next_repeat_event_ms)
			return;

		auto opt_keycode = s_scancode_to_keycode[m_repeat_scancode.value()];
		if (!opt_keycode.has_value())
			return;

		LibInput::RawKeyEvent event;
		event.keycode = opt_keycode.value();
		event.modifier = m_repeat_modifier | KeyModifier::Pressed;
		add_event(BAN::ConstByteSpan::from(event));

		m_next_repeat_event_ms += s_repeat_interval_ms;
	}

	void initialize_scancode_to_keycode()
	{
		using LibInput::keycode_function;
		using LibInput::keycode_normal;
		using LibInput::keycode_numpad;

		for (auto& mapping : s_scancode_to_keycode)
			mapping.clear();

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
		s_scancode_to_keycode[0xE0] = keycode_normal(4,  0);
		s_scancode_to_keycode[0xE3] = keycode_normal(4,  1);
		s_scancode_to_keycode[0xE2] = keycode_normal(4,  2);
		s_scancode_to_keycode[0x2C] = keycode_normal(4,  3);
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
		s_scancode_to_keycode[0x47] = keycode_function(20);

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

	constexpr bool is_repeatable_scancode(uint8_t scancode)
	{
		switch (scancode)
		{
			case 0x00: // reserved
			case 0x01: // error roll over
			case 0x02: // post fail
			case 0x03: // error undefined
			case 0x29: // escape
			case 0x39: // caps lock
			case 0x3A: // f1
			case 0x3B: // f2
			case 0x3C: // f3
			case 0x3D: // f4
			case 0x3E: // f5
			case 0x3F: // f6
			case 0x40: // f7
			case 0x41: // f8
			case 0x42: // f9
			case 0x43: // f10
			case 0x44: // f11
			case 0x45: // f12
			case 0x47: // scroll lock
			case 0x53: // num lock
			case 0xE0: // left control
			case 0xE1: // left shift
			case 0xE2: // left alt
			case 0xE3: // left super
			case 0xE4: // right control
			case 0xE5: // right shift
			case 0xE6: // right alt
			case 0xE7: // right super
				return false;
			default:
				return true;
		}
	}

}
