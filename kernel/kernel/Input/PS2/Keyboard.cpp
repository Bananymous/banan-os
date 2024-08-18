#include <BAN/ScopeGuard.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Input/PS2/Config.h>
#include <kernel/Input/PS2/Keyboard.h>
#include <kernel/Thread.h>
#include <LibInput/KeyboardLayout.h>
#include <LibInput/KeyEvent.h>

namespace Kernel::Input
{

	BAN::ErrorOr<BAN::RefPtr<PS2Keyboard>> PS2Keyboard::create(PS2Controller& controller, uint8_t scancode_set)
	{
		auto keyboard = TRY(BAN::RefPtr<PS2Keyboard>::create(controller, scancode_set != 0));
		if (scancode_set)
		{
			if (scancode_set > 3)
			{
				dwarnln("Invalid scancode set {}, using scan code set 2 instead", scancode_set);
				scancode_set = 2;
			}
			keyboard->m_scancode_set = scancode_set;
			keyboard->m_keymap.initialize(scancode_set);
			dprintln("Skipping scancode detection, using scan code set {}", scancode_set);
		}
		return keyboard;
	}

	PS2Keyboard::PS2Keyboard(PS2Controller& controller, bool basic)
		: PS2Device(controller, InputDevice::Type::Keyboard)
		, m_basic(basic)
	{ }

	void PS2Keyboard::send_initialize()
	{
		constexpr uint8_t wanted_scancode_set = 3;
		update_leds();
		if (m_scancode_set == 0xFF)
		{
			append_command_queue(Command::CONFIG_SCANCODE_SET, wanted_scancode_set, 0);
			append_command_queue(Command::CONFIG_SCANCODE_SET, 0, 1);
		}
		else
		{
			append_command_queue(PS2::DeviceCommand::ENABLE_SCANNING, 0);
		}
	}

	void PS2Keyboard::command_timedout(uint8_t* command_data, uint8_t command_size)
	{
		if (command_size == 0)
			return;

		if (command_data[0] == Command::CONFIG_SCANCODE_SET && m_scancode_set >= 0xFE)
		{
			dwarnln("Could not detect scancode set, assuming 2");
			m_scancode_set = 2;
			m_keymap.initialize(m_scancode_set);
			append_command_queue(PS2::DeviceCommand::ENABLE_SCANNING, 0);
		}
	}

	void PS2Keyboard::handle_byte(uint8_t byte)
	{
		using LibInput::Key;
		using LibInput::RawKeyEvent;
		using KeyModifier = LibInput::KeyEvent::Modifier;

		if (byte == PS2::KBResponse::KEY_ERROR_OR_BUFFER_OVERRUN1 || byte == PS2::KBResponse::KEY_ERROR_OR_BUFFER_OVERRUN2)
		{
			dwarnln("Key detection error or internal buffer overrun");
			return;
		}

		if (m_scancode_set == 0xFF)
		{
			append_command_queue(Command::CONFIG_SCANCODE_SET, 0, 1);
			m_scancode_set = 0xFE;
			return;
		}

		if (m_scancode_set == 0xFE)
		{
			if (1 <= byte && byte <= 3)
			{
				m_scancode_set = byte;
				dprintln("Using scancode set {}", m_scancode_set);
			}
			else
			{
				dwarnln("Could not detect scancode set, assuming 1");
				m_scancode_set = 1;
			}
			m_keymap.initialize(m_scancode_set);
			append_command_queue(PS2::DeviceCommand::ENABLE_SCANNING, 0);
			return;
		}

		if (m_byte_index >= 3)
		{
			dwarnln("PS/2 corrupted key packet");
			m_byte_index = 0;
			return;
		}

		m_byte_buffer[m_byte_index++] = byte;
		if (byte == 0xE0)
			return;
		if ((m_scancode_set == 2 || m_scancode_set == 3) && byte == 0xF0)
			return;

		bool extended = false;
		bool released = false;

		uint8_t index = 0;
		// in all scancode sets, extended scancode is indicated by byte 0xE0
		if (index < m_byte_index && m_byte_buffer[index] == 0xE0)
		{
			extended = true;
			index++;
		}
		// in scancode set 1, released key is indicated by bit 7 set
		if (m_scancode_set == 1 && (m_byte_buffer[index] & 0x80))
		{
			released = true;
			m_byte_buffer[index] &= 0x7F;
		}
		// in scancode set 2 and 3, released key is indicated by byte 0xF0
		if ((m_scancode_set == 2 || m_scancode_set == 3) && m_byte_buffer[index] == 0xF0)
		{
			released = true;
			index++;
		}

		bool corrupted = (index + 1 != m_byte_index);
		m_byte_index = 0;

		if (corrupted)
		{
			dwarnln("PS/2 corrupted key packet");
			return;
		}

		auto keycode = m_keymap.get_keycode(m_byte_buffer[index], extended);
		if (!keycode.has_value())
			return;

		auto dummy_event = LibInput::KeyboardLayout::get().key_event_from_raw(RawKeyEvent { .modifier = 0, .keycode = keycode.value() });

		uint16_t modifier_mask = 0;
		uint16_t toggle_mask = 0;
		switch (dummy_event.key)
		{
			case Key::LeftShift:	modifier_mask = KeyModifier::LShift;	break;
			case Key::RightShift:	modifier_mask = KeyModifier::RShift;	break;
			case Key::LeftCtrl:		modifier_mask = KeyModifier::LCtrl;		break;
			case Key::RightCtrl:	modifier_mask = KeyModifier::RCtrl;		break;
			case Key::LeftAlt:		modifier_mask = KeyModifier::LAlt;		break;
			case Key::RightAlt:		modifier_mask = KeyModifier::RAlt;		break;

			case Key::ScrollLock:	toggle_mask = KeyModifier::ScrollLock;	break;
			case Key::NumLock:		toggle_mask = KeyModifier::NumLock;		break;
			case Key::CapsLock:		toggle_mask = KeyModifier::CapsLock;	break;

			default: break;
		}

		if (modifier_mask)
		{
			if (released)
				m_modifiers &= ~modifier_mask;
			else
				m_modifiers |= modifier_mask;
		}

		if (toggle_mask && !released)
		{
			m_modifiers ^= toggle_mask;
			update_leds();
		}

		RawKeyEvent event;
		event.modifier = m_modifiers | (released ? 0 : KeyModifier::Pressed);
		event.keycode = keycode.value();
		add_event(BAN::ConstByteSpan::from(event));
	}

	void PS2Keyboard::update_leds()
	{
		using KeyModifier = LibInput::KeyEvent::Modifier;

		if (m_basic)
			return;

		uint8_t new_leds = 0;
		if (m_modifiers & +KeyModifier::ScrollLock)
			new_leds |= PS2::KBLeds::SCROLL_LOCK;
		if (m_modifiers & +KeyModifier::NumLock)
			new_leds |= PS2::KBLeds::NUM_LOCK;
		if (m_modifiers & +KeyModifier::CapsLock)
			new_leds |= PS2::KBLeds::CAPS_LOCK;
		append_command_queue(Command::SET_LEDS, new_leds, 0);
	}

}
