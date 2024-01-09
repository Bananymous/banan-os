#include <BAN/ScopeGuard.h>
#include <kernel/CriticalScope.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Input/PS2/Config.h>
#include <kernel/Input/PS2/Keyboard.h>
#include <kernel/Thread.h>

#define SET_MASK(byte, mask, on_off) ((on_off) ? ((byte) | (mask)) : ((byte) & ~(mask)))
#define TOGGLE_MASK(byte, mask) ((byte) ^ (mask))

namespace Kernel::Input
{

	BAN::ErrorOr<PS2Keyboard*> PS2Keyboard::create(PS2Controller& controller)
	{
		PS2Keyboard* keyboard = new PS2Keyboard(controller);
		if (keyboard == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return keyboard;
	}

	PS2Keyboard::PS2Keyboard(PS2Controller& controller)
		: PS2Device(controller)
	{ }

	void PS2Keyboard::send_initialize()
	{
		constexpr uint8_t wanted_scancode_set = 3;
		append_command_queue(Command::SET_LEDS, 0x00, 0);
		append_command_queue(Command::CONFIG_SCANCODE_SET, wanted_scancode_set, 0);
		append_command_queue(Command::CONFIG_SCANCODE_SET, 0, 1);
	}

	void PS2Keyboard::command_timedout(uint8_t* command_data, uint8_t command_size)
	{
		if (command_size == 0)
			return;

		if (command_data[0] == Command::CONFIG_SCANCODE_SET && m_scancode_set >= 0xFE)
		{
			dwarnln("Could not detect scancode set, assuming 1");
			m_scancode_set = 1;
			m_keymap.initialize(m_scancode_set);
			append_command_queue(PS2::DeviceCommand::ENABLE_SCANNING, 0);
		}
	}

	void PS2Keyboard::handle_byte(uint8_t byte)
	{
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

		uint16_t modifier_mask = 0;
		uint16_t toggle_mask = 0;
		switch (keycode.value())
		{
			case ModifierKeycode::LShift:	modifier_mask = KeyEvent::Modifier::LShift;	break;
			case ModifierKeycode::RShift:	modifier_mask = KeyEvent::Modifier::RShift;	break;
			case ModifierKeycode::LCtrl:	modifier_mask = KeyEvent::Modifier::LCtrl;	break;
			case ModifierKeycode::RCtrl:	modifier_mask = KeyEvent::Modifier::RCtrl;	break;
			case ModifierKeycode::LAlt:		modifier_mask = KeyEvent::Modifier::LAlt;	break;
			case ModifierKeycode::RAlt:		modifier_mask = KeyEvent::Modifier::RAlt;	break;

			case ModifierKeycode::ScrollLock:	toggle_mask = KeyEvent::Modifier::ScrollLock;	break;
			case ModifierKeycode::NumLock:		toggle_mask = KeyEvent::Modifier::NumLock;		break;
			case ModifierKeycode::CapsLock:		toggle_mask = KeyEvent::Modifier::CapsLock;		break;
		}

		if (modifier_mask)
		{
			if (released)
				m_modifiers &= ~modifier_mask;
			else
				m_modifiers |= modifier_mask;
		}

		if (toggle_mask)
		{
			m_modifiers ^= toggle_mask;
			update_leds();
		}

		KeyEvent event;
		event.modifier = m_modifiers | (released ? 0 : KeyEvent::Modifier::Pressed);
		event.keycode = keycode.value();

		if (m_event_queue.full())
		{
			dwarnln("PS/2 event queue full");
			m_event_queue.pop();
		}
		m_event_queue.push(event);

		m_semaphore.unblock();
	}

	void PS2Keyboard::update_leds()
	{
		uint8_t new_leds = 0;
		if (m_modifiers & +Input::KeyEvent::Modifier::ScrollLock)
			new_leds |= PS2::KBLeds::SCROLL_LOCK;
		if (m_modifiers & +Input::KeyEvent::Modifier::NumLock)
			new_leds |= PS2::KBLeds::NUM_LOCK;
		if (m_modifiers & +Input::KeyEvent::Modifier::CapsLock)
			new_leds |= PS2::KBLeds::CAPS_LOCK;
		append_command_queue(Command::SET_LEDS, new_leds, 0);
	}

	BAN::ErrorOr<size_t> PS2Keyboard::read_impl(off_t, BAN::ByteSpan buffer)
	{
		if (buffer.size() < sizeof(KeyEvent))
			return BAN::Error::from_errno(ENOBUFS);

		while (true)
		{
			if (m_event_queue.empty())
				TRY(Thread::current().block_or_eintr(m_semaphore));

			CriticalScope _;
			if (m_event_queue.empty())
				continue;

			buffer.as<KeyEvent>() = m_event_queue.front();
			m_event_queue.pop();

			return sizeof(KeyEvent);
		}
	}

	bool PS2Keyboard::has_data_impl() const
	{
		CriticalScope _;
		return !m_event_queue.empty();
	}

}
