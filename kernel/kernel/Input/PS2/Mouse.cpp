#include <BAN/ScopeGuard.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Input/PS2/Config.h>
#include <kernel/Input/PS2/Mouse.h>
#include <kernel/Thread.h>
#include <LibInput/MouseEvent.h>

#define SET_MASK(byte, mask, on_off) ((on_off) ? ((byte) | (mask)) : ((byte) & ~(mask)))
#define TOGGLE_MASK(byte, mask) ((byte) ^ (mask))

namespace Kernel::Input
{

	BAN::ErrorOr<BAN::RefPtr<PS2Mouse>> PS2Mouse::create(PS2Controller& controller)
	{
		return TRY(BAN::RefPtr<PS2Mouse>::create(controller));
	}

	PS2Mouse::PS2Mouse(PS2Controller& controller)
		: PS2Device(controller, InputDevice::Type::Mouse)
	{ }

	void PS2Mouse::send_initialize()
	{
		// Query extensions
		append_command_queue(Command::SET_SAMPLE_RATE, 200, 0);
		append_command_queue(Command::SET_SAMPLE_RATE, 100, 0);
		append_command_queue(Command::SET_SAMPLE_RATE, 80, 0);
		append_command_queue(PS2::DeviceCommand::IDENTIFY, 1);
	}

	void PS2Mouse::initialize_extensions(uint8_t byte)
	{
		ASSERT(!m_enabled);

		switch (byte)
		{
			case 0x00:
				m_mouse_id = 0x00;
				m_enabled = true;
				break;
			case 0x03:
				if (m_mouse_id == 0x03)
					m_enabled = true;
				else
				{
					m_mouse_id = 0x03;
					append_command_queue(Command::SET_SAMPLE_RATE, 200, 0);
					append_command_queue(Command::SET_SAMPLE_RATE, 200, 0);
					append_command_queue(Command::SET_SAMPLE_RATE, 80, 0);
					append_command_queue(PS2::DeviceCommand::IDENTIFY, 1);
				}
				break;
			case 0x04:
				m_mouse_id = 0x04;
				m_enabled = true;
				break;
			default:
				dprintln("PS/2 Mouse: unknown id {2H}", byte);
				break;
		}

		if (m_enabled)
		{
			append_command_queue(Command::SET_SAMPLE_RATE, 100, 0);
			append_command_queue(PS2::DeviceCommand::ENABLE_SCANNING, 0);
		}
	}

	void PS2Mouse::handle_byte(uint8_t byte)
	{
		using LibInput::MouseButton;
		using LibInput::MouseEvent;
		using LibInput::MouseEventType;

		if (!m_enabled)
			return initialize_extensions(byte);

		m_byte_buffer[m_byte_index++] = byte;
		if (!(m_byte_buffer[0] & 0x08))
		{
			dprintln("PS/2 mouse: corrupted package");
			m_byte_index = 0;
		}

		int packet_size = (m_mouse_id == 0x00) ? 3 : 4;
		if (m_byte_index < packet_size)
			return;

		// Ignore packets with bits 6 or 7 set. Qemu sends weird
		// non-standard packets on touchpad when scrolling horizontally.
		if (m_mouse_id == 0x04 && (m_byte_buffer[3] & 0xC0))
		{
			m_byte_index = 0;
			return;
		}

		uint8_t new_button_mask = m_byte_buffer[0] & 0x07;
		int32_t rel_x = m_byte_buffer[1] - (((uint16_t)m_byte_buffer[0] << 4) & 0x100);
		int32_t rel_y = m_byte_buffer[2] - (((uint16_t)m_byte_buffer[0] << 3) & 0x100);
		int32_t rel_z = 0;

		if (m_mouse_id == 0x03 || m_mouse_id == 0x04)
			rel_z = (m_byte_buffer[3] & 0x0F) - ((m_byte_buffer[3] << 1) & 0x10);

		if (m_mouse_id == 0x04)
			new_button_mask |= (m_byte_buffer[3] >> 1) & 0x18;

		m_byte_index = 0;

		constexpr MouseButton bit_to_button[] {
			MouseButton::Left,
			MouseButton::Right,
			MouseButton::Middle,
			MouseButton::Extra1,
			MouseButton::Extra2,
		};

		if (new_button_mask != m_button_mask)
		{
			for (int i = 0; i < 5; i++)
			{
				if ((new_button_mask & (1 << i)) == (m_button_mask & (1 << i)))
					continue;

				MouseEvent event;
				event.type = MouseEventType::MouseButtonEvent;
				event.button_event.button = bit_to_button[i];
				event.button_event.pressed = !!(new_button_mask & (1 << i));
				add_event(BAN::ConstByteSpan::from(event));
			}

			m_button_mask = new_button_mask;
		}

		if (rel_x || rel_y)
		{
			MouseEvent event;
			event.type = MouseEventType::MouseMoveEvent;
			event.move_event.rel_x = rel_x;
			event.move_event.rel_y = rel_y;
			add_event(BAN::ConstByteSpan::from(event));
		}

		if (rel_z)
		{
			MouseEvent event;
			event.type = MouseEventType::MouseScrollEvent;
			event.scroll_event.scroll = rel_z;
			add_event(BAN::ConstByteSpan::from(event));
		}
	}

}
