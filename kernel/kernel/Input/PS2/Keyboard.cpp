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
		append_command_queue(Command::SET_LEDS, 0x00);
		append_command_queue(Command::SCANCODE, PS2::KBScancode::SET_SCANCODE_SET2);
		append_command_queue(PS2::DeviceCommand::ENABLE_SCANNING);
	}

	void PS2Keyboard::handle_device_command_response(uint8_t byte)
	{
		switch (byte)
		{
			case PS2::KBResponse::KEY_ERROR_OR_BUFFER_OVERRUN1:
			case PS2::KBResponse::KEY_ERROR_OR_BUFFER_OVERRUN2:
				dwarnln("Key detection error or internal buffer overrun");
				break;
			default:
				dwarnln("Unhandeled byte {2H}", byte);
				break;
		}
	}

	void PS2Keyboard::handle_byte(uint8_t byte)
	{
		m_byte_buffer[m_byte_index++] = byte;
		if (byte == 0xE0 || byte == 0xF0)
			return;

		uint32_t scancode = 0;
		bool extended = false;
		bool released = false;

		for (uint8_t i = 0; i < m_byte_index; i++)
		{
			if (m_byte_buffer[i] == 0xE0)
				extended = true;
			else if (m_byte_buffer[i] == 0xF0)
				released = true;
			else
				scancode = (scancode << 8) | m_byte_buffer[i];
		}

		if (extended)
			scancode |= 0x80000000;

		m_byte_index = 0;

		Key key = m_keymap.key_for_scancode_and_modifiers(scancode, m_modifiers);

		if (key == Key::None)
			return;

		if (key == Input::Key::Invalid)
		{
			dprintln("unknown key for scancode {2H} {}", scancode & 0x7FFFFFFF, extended ? 'E' : ' ');
			return;
		}

		uint8_t modifier_mask = 0;
		uint8_t toggle_mask = 0;
		switch (key)
		{
			case Input::Key::LeftShift:
			case Input::Key::RightShift:
				modifier_mask = (uint8_t)Input::KeyEvent::Modifier::Shift;
				break;
			case Input::Key::LeftCtrl:
			case Input::Key::RightCtrl:
				modifier_mask = (uint8_t)Input::KeyEvent::Modifier::Ctrl;
				break;
			case Input::Key::Alt:
				modifier_mask = (uint8_t)Input::KeyEvent::Modifier::Alt;
				break;
			case Input::Key::AltGr:
				modifier_mask = (uint8_t)Input::KeyEvent::Modifier::AltGr;
				break;;
			case Input::Key::ScrollLock:
				toggle_mask = (uint8_t)Input::KeyEvent::Modifier::ScrollLock;
				break;
			case Input::Key::NumLock:
				toggle_mask = (uint8_t)Input::KeyEvent::Modifier::NumLock;
				break;
			case Input::Key::CapsLock:
				toggle_mask = (uint8_t)Input::KeyEvent::Modifier::CapsLock;
				break;
			default:
				break;
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

		Input::KeyEvent event;
		event.modifier = m_modifiers | (released ? (uint8_t)Input::KeyEvent::Modifier::Released : 0);
		event.key = key;

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
		if (m_modifiers & (uint8_t)Input::KeyEvent::Modifier::ScrollLock)
			new_leds |= PS2::KBLeds::SCROLL_LOCK;
		if (m_modifiers & (uint8_t)Input::KeyEvent::Modifier::NumLock)
			new_leds |= PS2::KBLeds::NUM_LOCK;
		if (m_modifiers & (uint8_t)Input::KeyEvent::Modifier::CapsLock)
			new_leds |= PS2::KBLeds::CAPS_LOCK;
		append_command_queue(Command::SET_LEDS, new_leds);
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
