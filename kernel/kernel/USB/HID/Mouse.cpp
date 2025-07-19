#include <kernel/USB/HID/Mouse.h>
#include <LibInput/MouseEvent.h>

namespace Kernel
{

	void USBMouse::start_report()
	{
		m_wheel = 0;
		m_rel_x = 0;
		m_rel_y = 0;

		m_abs_x = {};
		m_abs_y = {};

		for (auto& val : m_button_state_temp)
			val = false;
	}

	void USBMouse::stop_report()
	{
		if (m_abs_x.valid() && m_abs_y.valid())
		{
			dprintln_if(DEBUG_USB_MOUSE, "Mouse move absolute event {}, {}", m_abs_x.val, m_abs_y.val);

			LibInput::MouseEvent event;
			event.type = LibInput::MouseEventType::MouseMoveAbsEvent;
			event.move_abs_event.abs_x = m_abs_x.val;
			event.move_abs_event.min_x = m_abs_x.min;
			event.move_abs_event.max_x = m_abs_x.max;
			event.move_abs_event.abs_y = m_abs_y.val;
			event.move_abs_event.min_y = m_abs_y.min;
			event.move_abs_event.max_y = m_abs_y.max;
			add_event(BAN::ConstByteSpan::from(event));
		}

		if (m_rel_x || m_rel_y)
		{
			dprintln_if(DEBUG_USB_MOUSE, "Mouse move event {}, {}", m_rel_x, m_rel_y);

			LibInput::MouseEvent event;
			event.type = LibInput::MouseEventType::MouseMoveEvent;
			event.move_event.rel_x =  m_rel_x;
			event.move_event.rel_y = -m_rel_y;
			add_event(BAN::ConstByteSpan::from(event));
		}

		if (m_wheel)
		{
			dprintln_if(DEBUG_USB_MOUSE, "Mouse scroll event {}", m_wheel);

			LibInput::MouseEvent event;
			event.type = LibInput::MouseEventType::MouseScrollEvent;
			event.scroll_event.scroll = m_wheel;
			add_event(BAN::ConstByteSpan::from(event));
		}

		for (size_t i = 0; i < m_button_state.size(); i++)
		{
			if (m_button_state[i] == m_button_state_temp[i])
				continue;

			const bool pressed = m_button_state_temp[i];

			dprintln_if(DEBUG_USB_MOUSE, "Mouse button event {}: {}", i, pressed);

			LibInput::MouseEvent event;
			event.type = LibInput::MouseEventType::MouseButtonEvent;
			event.button_event.pressed = pressed;
			event.button_event.button = static_cast<LibInput::MouseButton>(i);
			add_event(BAN::ConstByteSpan::from(event));

			m_button_state[i] = m_button_state_temp[i];
		}
	}

	void USBMouse::handle_array(uint16_t usage_page, uint16_t usage)
	{
		dprintln_if(DEBUG_USB_MOUSE, "Unhandled array report {2H}:{2H}", usage_page, usage);
	}

	void USBMouse::handle_variable(uint16_t usage_page, uint16_t usage, int64_t state)
	{
		switch (usage_page)
		{
			case 0x01: // pointer
				switch (usage)
				{
					case 0x30:
						m_rel_x = state;
						break;
					case 0x31:
						m_rel_y = state;
						break;
					case 0x38:
						m_wheel = state;
						break;
					default:
						dprintln_if(DEBUG_USB_MOUSE, "Unsupported relative mouse usage {2H} on page {2H}", usage, usage_page);
						break;
				}
				break;
			default:
				dprintln_if(DEBUG_USB_MOUSE, "Unsupported relative mouse usage page {2H}", usage_page);
				break;
		}
	}

	void USBMouse::handle_variable_absolute(uint16_t usage_page, uint16_t usage, int64_t state, int64_t min, int64_t max)
	{
		(void)min; (void)max;

		switch (usage_page)
		{
			case 0x01: // pointer
				switch (usage)
				{
					case 0x30:
						m_abs_x = {
							.val = state,
							.min = min,
							.max = max,
						};
						break;
					case 0x31:
						m_abs_y = {
							.val = state,
							.min = min,
							.max = max,
						};
						break;
					default:
						dprintln_if(DEBUG_USB_MOUSE, "Unsupported absolute mouse usage {2H} on page {2H}", usage, usage_page);
						break;
				}
				break;
			case 0x09: // button
				if (usage == 0 || usage > m_button_state_temp.size())
					break;
				m_button_state_temp[usage - 1] = state;
				break;
			default:
				dprintln_if(DEBUG_USB_MOUSE, "Unsupported absolute mouse usage page {2H}", usage_page);
				break;
		}
	}

}
