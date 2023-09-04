#include <BAN/ScopeGuard.h>
#include <kernel/CriticalScope.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Input/PS2Keyboard.h>
#include <kernel/Timer/Timer.h>

#include <sys/sysmacros.h>

#define SET_MASK(byte, mask, on_off) ((on_off) ? ((byte) | (mask)) : ((byte) & ~(mask)))
#define TOGGLE_MASK(byte, mask) ((byte) ^ (mask))

namespace Kernel::Input
{

	namespace PS2
	{

		enum Response
		{
			KEY_ERROR_OR_BUFFER_OVERRUN1 = 0x00,
			SELF_TEST_PASSED = 0xAA,
			ECHO_RESPONSE = 0xEE,
			ACK = 0xFA,
			RESEND = 0xFE,
			KEY_ERROR_OR_BUFFER_OVERRUN2 = 0xFF,
		};

		enum Scancode
		{
			SET_SCANCODE_SET1 = 1,
			SET_SCANCODE_SET2 = 2,
			SET_SCANCODE_SET3 = 3,
		};

		enum Leds
		{
			SCROLL_LOCK	= (1 << 0),
			NUM_LOCK	= (1 << 1),
			CAPS_LOCK	= (1 << 2),
		};

	}

	BAN::ErrorOr<PS2Keyboard*> PS2Keyboard::create(PS2Controller& controller)
	{
		PS2Keyboard* keyboard = new PS2Keyboard(controller);
		if (keyboard == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		BAN::ScopeGuard guard([keyboard] { delete keyboard; });
		TRY(keyboard->initialize());
		guard.disable();
		return keyboard;
	}

	PS2Keyboard::PS2Keyboard(PS2Controller& controller)
		: m_controller(controller)
		, m_rdev(makedev(DevFileSystem::get().get_next_dev(), 0))
	{ }

	void PS2Keyboard::on_byte(uint8_t byte)
	{
		// NOTE: This implementation does not allow using commands
		//       that respond with more bytes than ACK
		switch (m_state)
		{
			case State::WaitingAck:
			{
				switch (byte)
				{
					case PS2::Response::ACK:
						m_command_queue.pop();
						m_state = State::Normal;
						break;
					case PS2::Response::RESEND:
						m_state = State::Normal;
						break;
					case PS2::Response::KEY_ERROR_OR_BUFFER_OVERRUN1:
					case PS2::Response::KEY_ERROR_OR_BUFFER_OVERRUN2:
						dwarnln("Key detection error or internal buffer overrun");
						break;
					default:
						dwarnln("Unhandeled byte {2H}", byte);
						break;
				}
				break;
			}
			case State::Normal:
			{
				m_byte_buffer[m_byte_index++] = byte;
				if (byte != 0xE0 && byte != 0xF0)
					buffer_has_key();
				break;
			}
		}
	}

	BAN::ErrorOr<void> PS2Keyboard::initialize()
	{
		append_command_queue(Command::SET_LEDS, 0x00);
		append_command_queue(Command::SCANCODE, PS2::Scancode::SET_SCANCODE_SET2);
		append_command_queue(Command::ENABLE_SCANNING);
		return {};
	}

	void PS2Keyboard::update()
	{
		if (m_state == State::WaitingAck)
			return;
		if (m_command_queue.empty())
			return;
		m_state = State::WaitingAck;
		m_controller.send_byte(this, m_command_queue.front());
	}

	void PS2Keyboard::append_command_queue(uint8_t byte)
	{
		if (m_command_queue.full())
		{
			dwarnln("PS/2 command queue full");
			return;
		}
		m_command_queue.push(byte);
	}
	
	void PS2Keyboard::append_command_queue(uint8_t byte1, uint8_t byte2)
	{
		if (m_command_queue.size() + 2 > m_command_queue.capacity())
		{
			dwarnln("PS/2 command queue full");
			return;
		}
		m_command_queue.push(byte1);
		m_command_queue.push(byte2);
	}

	void PS2Keyboard::buffer_has_key()
	{
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

		if (event.pressed() && event.key == Input::Key::F11)
		{
			auto time = SystemTimer::get().time_since_boot();
			dprintln("{}.{9} s", time.tv_sec, time.tv_nsec);
		}

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
			new_leds |= PS2::Leds::SCROLL_LOCK;
		if (m_modifiers & (uint8_t)Input::KeyEvent::Modifier::NumLock)
			new_leds |= PS2::Leds::NUM_LOCK;
		if (m_modifiers & (uint8_t)Input::KeyEvent::Modifier::CapsLock)
			new_leds |= PS2::Leds::CAPS_LOCK;
		append_command_queue(Command::SET_LEDS, new_leds);
	}

	BAN::ErrorOr<size_t> PS2Keyboard::read(size_t, void* buffer, size_t size)
	{
		if (size < sizeof(KeyEvent))
			return BAN::Error::from_errno(ENOBUFS);

		while (true)
		{
			if (m_event_queue.empty())
				m_semaphore.block();

			CriticalScope _;
			if (m_event_queue.empty())
				continue;

			*(KeyEvent*)buffer = m_event_queue.front();
			m_event_queue.pop();

			return sizeof(KeyEvent);
		}
	}

}