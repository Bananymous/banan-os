#include <kernel/Input/InputDevice.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/USB/HID/Joystick.h>

#include <sys/ioctl.h>

namespace Kernel
{

	USBJoystick::USBJoystick(USBHIDDriver& driver)
		: USBHIDDevice(InputDevice::Type::Joystick)
		, m_driver(driver)
	{
		using namespace LibInput;
		m_state.axis[JSA_TRIGGER_LEFT] = -32767;
		m_state.axis[JSA_TRIGGER_RIGHT] = -32767;
	}

	void USBJoystick::initialize_type()
	{
		m_type = Type::Unknown;

		const auto& device_descriptor = m_driver.device().device_descriptor();
		switch (device_descriptor.idVendor)
		{
			case 0x054C: // Sony
				switch (device_descriptor.idProduct)
				{
					case 0x0268: // DualShock 3
						m_type = Type::DualShock3;
						break;
				}
				break;
		}
	}

	BAN::ErrorOr<void> USBJoystick::initialize()
	{
		initialize_type();

		m_send_buffer = TRY(DMARegion::create(PAGE_SIZE));

		USBDeviceRequest request;

		switch (m_type)
		{
			case Type::Unknown:
				break;
			case Type::DualShock3:
			{
				// linux hid-sony.c

				// move ps3 controller to "operational" state
				request.bmRequestType = USB::RequestType::DeviceToHost | USB::RequestType::Class | USB::RequestType::Interface;
				request.bRequest      = 0x01;
				request.wValue        = 0x03F2;
				request.wIndex        = m_driver.interface().descriptor.bInterfaceNumber;
				request.wLength       = 17;
				TRY(m_driver.device().send_request(request, m_send_buffer->paddr()));

				// some compatible controllers need this too (we don't detect compatible controllers though)
				request.bmRequestType = USB::RequestType::DeviceToHost | USB::RequestType::Class | USB::RequestType::Interface;
				request.bRequest      = 0x01;
				request.wValue        = 0x03F5;
				request.wIndex        = m_driver.interface().descriptor.bInterfaceNumber;
				request.wLength       = 8;
				TRY(m_driver.device().send_request(request, m_send_buffer->paddr()));

				break;
			}
		}

		return {};
	}

	void USBJoystick::start_report()
	{
		m_interrupt_state = m_state_lock.lock();
		m_state_index = 0;
	}

	void USBJoystick::stop_report()
	{
		m_state_lock.unlock(m_interrupt_state);
		m_has_got_report = true;
	}

	void USBJoystick::handle_array(uint16_t usage_page, uint16_t usage)
	{
		(void)usage;
		dprintln("Unsupported array {2H}", usage_page);
	}

	void USBJoystick::handle_variable(uint16_t usage_page, uint16_t usage, int64_t state)
	{
		(void)usage;
		(void)state;
		dprintln("Unsupported relative usage page {2H}", usage_page);
	}

	void USBJoystick::handle_variable_absolute(uint16_t usage_page, uint16_t usage, int64_t state, int64_t min, int64_t max)
	{
		using namespace LibInput;

		constexpr auto map_joystick_axis =
			[](int64_t value, int64_t min, int64_t max) -> int16_t
			{
				if (min == max)
					return 0;
				return (value - min) * 65534 / (max - min) - 32767;
			};

		constexpr auto map_trigger_axis =
			[](int64_t value, int64_t min, int64_t max) -> int16_t
			{
				if (min == max)
					return -32767;
				return (value - min) * 65534 / (max - min) - 32767;
			};

		static constexpr JoystickButton button_map[] {
			[ 0] = JSB_SELECT,
			[ 1] = JSB_STICK_LEFT,
			[ 2] = JSB_STICK_RIGHT,
			[ 3] = JSB_START,
			[ 4] = JSB_DPAD_UP,
			[ 5] = JSB_DPAD_RIGHT,
			[ 6] = JSB_DPAD_DOWN,
			[ 7] = JSB_DPAD_LEFT,
			[ 8] = JSB_COUNT, // left trigger
			[ 9] = JSB_COUNT, // right trigger
			[10] = JSB_SHOULDER_LEFT,
			[11] = JSB_SHOULDER_RIGHT,
			[12] = JSB_FACE_UP,
			[13] = JSB_FACE_RIGHT,
			[14] = JSB_FACE_DOWN,
			[15] = JSB_FACE_LEFT,
			[16] = JSB_MENU,
		};

		switch (usage_page)
		{
			case 0x01:
				// TODO: These are probably only correct for dualshock 3
				switch (usage)
				{
					case 0x01:
						if (m_state_index == 8)
							m_state.axis[JSA_TRIGGER_LEFT] = map_trigger_axis(state, min, max);
						if (m_state_index == 9)
							m_state.axis[JSA_TRIGGER_RIGHT] = map_trigger_axis(state, min, max);
						m_state_index++;
						break;
					case 0x30:
						m_state.axis[JSA_STICK_LEFT_X] = map_joystick_axis(state, min, max);
						break;
					case 0x31:
						m_state.axis[JSA_STICK_LEFT_Y] = map_joystick_axis(state, min, max);
						break;
					case 0x32:
						m_state.axis[JSA_STICK_RIGHT_X] = map_joystick_axis(state, min, max);
						break;
					case 0x35:
						m_state.axis[JSA_STICK_RIGHT_Y] = map_joystick_axis(state, min, max);
						break;
				}
				break;
			case 0x09:
				// TODO: These are probably only correct for dualshock 3
				if (usage > 0 && usage <= sizeof(button_map) / sizeof(*button_map))
					if (const auto button = button_map[usage - 1]; button != JSB_COUNT)
						m_state.buttons[button] = state;
				break;
			default:
				dprintln("Unsupported absolute usage page {2H}", usage_page);
				break;
		}
	}

	BAN::ErrorOr<size_t> USBJoystick::read_impl(off_t, BAN::ByteSpan buffer)
	{
		SpinLockGuard _(m_state_lock);
		const size_t to_copy = BAN::Math::min(buffer.size(), sizeof(m_state));
		memcpy(buffer.data(), &m_state, to_copy);
		return to_copy;
	}

	BAN::ErrorOr<long> USBJoystick::ioctl_impl(int request, void* arg)
	{
		switch (request)
		{
			case JOYSTICK_GET_LEDS:
				*static_cast<uint8_t*>(arg) = m_led_state;
				return 0;
			case JOYSTICK_SET_LEDS:
				switch (m_type)
				{
					case Type::Unknown:
						return BAN::Error::from_errno(ENOTSUP);
					case Type::DualShock3:
						TRY(update_dualshock3_state(*static_cast<const uint8_t*>(arg), m_rumble_strength));
						return 0;
				}
				ASSERT_NOT_REACHED();
			case JOYSTICK_GET_RUMBLE:
				*static_cast<uint32_t*>(arg) = m_rumble_strength;
				return 0;
			case JOYSTICK_SET_RUMBLE:
				switch (m_type)
				{
					case Type::Unknown:
						return BAN::Error::from_errno(ENOTSUP);
					case Type::DualShock3:
						TRY(update_dualshock3_state(m_led_state, *static_cast<const uint32_t*>(arg)));
						return 0;
				}
				ASSERT_NOT_REACHED();
		}

		return USBHIDDevice::ioctl(request, arg);
	}

	void USBJoystick::update()
	{
		if (!m_has_got_report)
			return;

		switch (m_type)
		{
			case Type::Unknown:
				break;
			case Type::DualShock3:
				// DualShock 3 only accepts leds after it has started sending reports
				// (when you press the PS button)
				if (!m_has_initialized_leds)
					(void)update_dualshock3_state(m_led_state, m_rumble_strength);
				break;
		}
	}

	BAN::ErrorOr<void> USBJoystick::update_dualshock3_state(uint8_t led_state, uint8_t rumble_strength)
	{
		led_state &= 0x0F;

		LockGuard _(m_command_mutex);

		// we cannot do anything until we have received an reports
		if (!m_has_got_report)
		{
			m_led_state = led_state;
			m_rumble_strength = rumble_strength;
			return {};
		}

		auto* request_data = reinterpret_cast<uint8_t*>(m_send_buffer->vaddr());
		memset(request_data, 0, 35);

		// header
		request_data[0] = 0x01; // report id (?)
		request_data[1] = 0xFF; // no idea but linux sets this, doesn't seem to affect anything

		// first byte is maybe *enable rumble control*, it has to be non-zero for rumble to do anything
		request_data[3] = 0xFF;
		request_data[4] = rumble_strength;

		// LED bitmap (bit 1: led 1, bit 2: led 2, ...)
		request_data[9] = led_state << 1;

		// No idea what these do but they need to be correct for the corresponding led to turn on.
		// Also they are in reverse order, first entry corresponds to led 4, second to led 3, ...
		for (size_t i = 0; i < 4; i++)
		{
			// values are the same as linux sends
			request_data[10 + i * 5] = 0xFF; // has to be non zero
			request_data[11 + i * 5] = 0x27; // ignored
			request_data[12 + i * 5] = 0x10; // has to be non zero
			request_data[13 + i * 5] = 0x00; // ignored
			request_data[14 + i * 5] = 0x32; // has to be non zero
		}

		USBDeviceRequest request;
		request.bmRequestType = USB::RequestType::HostToDevice | USB::RequestType::Class | USB::RequestType::Interface;
		request.bRequest      = 0x09;
		request.wValue        = 0x0201;
		request.wIndex        = m_driver.interface().descriptor.bInterfaceNumber;
		request.wLength       = 35;
		TRY(m_driver.device().send_request(request, m_send_buffer->paddr()));

		m_led_state = led_state;
		m_rumble_strength = rumble_strength;
		m_has_initialized_leds = true;

		return {};
	}

}
